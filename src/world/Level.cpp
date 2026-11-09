// src/world/Level.cpp
// Builds and draws the university level. Read it top to bottom:
//   1. small helpers (materials, flicker)
//   2. Builder: functions that add boxes (walls, furniture, trees, vehicles ...)
//   3. Builder::build(): the actual map, area by area
//   4. Level::Level / Level::draw
#include "world/Level.h"
#include "world/ProceduralTextures.h"
#include "world/Sites.h"
#include "glad.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>

using V3 = glm::vec3;

namespace {

// ------------------------------------------------------------------ materials
struct Mat {
    const Texture* tex = nullptr;
    glm::vec2 uv{1.0f};
    V3 tint{1.0f};
    float shin = 16.0f, spec = 0.05f, emissive = 0.0f, alpha = 1.0f;
    int uvMode = 0, mode = 0, light = -1;
    bool solid = true;      // does the player collide with it?
};

Mat worldMat(const Texture* t, float scale, V3 tint = V3(1.0f), float shin = 16.0f, float spec = 0.05f)
{
    Mat m; m.tex = t; m.uv = glm::vec2(scale); m.tint = tint; m.shin = shin; m.spec = spec;
    return m;
}
Mat uvMat(const Texture* t, float u, float v, V3 tint = V3(1.0f), float shin = 16.0f, float spec = 0.05f)
{
    Mat m = worldMat(t, 1.0f, tint, shin, spec); m.uv = glm::vec2(u, v); m.uvMode = 1;
    return m;
}

// A decal (sign, notice, paper, fixture ...) is purely visual: the player walks through it.
Mat decal(Mat m) { m.solid = false; return m; }

enum class Zone { Outdoor, Room, Hall };
struct Opening { float a0, a1, y0, y1; };
struct Frame { V3 origin; float yaw; };   // a local coordinate system for furniture/vehicles

float rnd(unsigned int& s) { s = s * 1664525u + 1013904223u; return ((s >> 8) & 0xFFFF) / 65535.0f; }

float flickerValue(int type, float phase, float t)
{
    auto h = [](float x) { float v = std::sin(x * 12.9898f) * 43758.5453f; return v - std::floor(v); };
    switch (type) {
    case 1:  return 0.93f + 0.07f * std::sin(t * 3.0f + phase);
    case 2: {   // broken tube: long dark spells and stutters
        float a = h(std::floor(t * 14.0f + phase));
        float b = h(std::floor(t * 0.6f + phase * 3.0f));
        if (b < 0.35f) return a < 0.5f ? 0.0f : 0.6f;
        return a < 0.12f ? 0.1f : 1.0f;
    }
    case 3:  return 0.55f + 0.45f * (0.5f + 0.5f * std::sin(t * 2.2f + phase));
    default: return 1.0f;
    }
}

// ------------------------------------------------------------------ Builder
class Builder {
public:
    Builder(std::unordered_map<std::string, Texture>& tex, std::vector<Prop>& props, std::vector<PointLight>& lights,
            int& cardLight, int& checkpointLight)
        : m_tex(tex), m_props(props), m_lights(lights), m_cardLight(cardLight), m_checkpointLight(checkpointLight) {}
    void build();

private:
    std::unordered_map<std::string, Texture>& m_tex;
    std::vector<Prop>& m_props;
    std::vector<PointLight>& m_lights;
    int& m_cardLight;
    int& m_checkpointLight;
    int m_tag = 0;                 // mission tag given to every prop created while it is non-zero
    float m_sun = 1.0f, m_amb = 1.0f;
    unsigned int m_seed = 12345u;

    // materials
    Mat mWallRoom, mWallHall, mWallOut, mConcrete, mConcDark, mWood, mWoodDark, mWoodFloor, mDeskTop,
        mChair, mTile, mBrick, mGrass, mAsphalt, mPaving, mMetal, mMetalDark, mFoliage, mDoor, mFrame,
        mPaper, mGlass, mWinDark, mStair, mBoard, mFacade, mGate, mHazard, mFlag;

    // ---- basics
    const Texture* T(const char* n) { return &m_tex.at(n); }
    void add(const char* n, Texture t) { m_tex.emplace(std::string(n), std::move(t)); }
    void zone(Zone z)
    {
        if (z == Zone::Outdoor) { m_sun = 1.0f;  m_amb = 1.0f; }
        if (z == Zone::Room)    { m_sun = 0.12f; m_amb = 1.0f; }
        if (z == Zone::Hall)    { m_sun = 0.04f; m_amb = 0.55f; }
    }
    void loadTextures();
    void makeMats();

    void boxC(V3 c, V3 size, const Mat& m, V3 rot = V3(0.0f))
    {
        Prop p;
        p.pos = c; p.size = size; p.rot = rot; p.tex = m.tex; p.uv = m.uv; p.tint = m.tint;
        p.shininess = m.shin; p.specular = m.spec; p.sunScale = m_sun; p.ambientScale = m_amb;
        p.emissive = m.emissive; p.alpha = m.alpha; p.uvMode = m.uvMode; p.mode = m.mode; p.light = m.light;
        p.solid = m.solid;
        p.tag = m_tag;
        m_props.push_back(p);
    }
    void box(V3 mn, V3 mx, const Mat& m, V3 rot = V3(0.0f)) { boxC((mn + mx) * 0.5f, mx - mn, m, rot); }

    // A box placed in a local frame (so a whole desk or car can be moved/rotated as one)
    void part(const Frame& f, V3 lc, V3 size, const Mat& m, V3 lrot = V3(0.0f))
    {
        float th = glm::radians(f.yaw), c = std::cos(th), s = std::sin(th);
        V3 w(f.origin.x + lc.x * c + lc.z * s, f.origin.y + lc.y, f.origin.z - lc.x * s + lc.z * c);
        boxC(w, size, m, V3(lrot.x, f.yaw + lrot.y, lrot.z));
    }

    int addLight(V3 pos, V3 color, float radius, int flicker)
    {
        PointLight l; l.pos = pos; l.color = color; l.radius = radius; l.flicker = flicker;
        l.phase = (float)m_lights.size() * 1.37f;
        m_lights.push_back(l);
        return (int)m_lights.size() - 1;
    }
    Mat fixture(int light, V3 tint, float glow = 1.4f)
    {
        Mat m = worldMat(T("concrete"), 1.0f, tint, 8.0f, 0.0f); m.emissive = glow; m.light = light; m.solid = false; return m;
    }

    // ---- walls
    void wallSeg(bool alongX, float a, float b, float p0, float p1, float ya, float yb, const Mat& m)
    {
        if (b - a < 0.001f || yb - ya < 0.001f) return;
        if (alongX) box(V3(a, ya, p0), V3(b, yb, p1), m); else box(V3(p0, ya, a), V3(p1, yb, b), m);
    }
    void wall(bool alongX, float a0, float a1, float p0, float p1, float y0, float y1, const Mat& m,
              std::vector<Opening> ops = {})
    {
        std::sort(ops.begin(), ops.end(), [](const Opening& l, const Opening& r) { return l.a0 < r.a0; });
        float cur = a0;
        for (const Opening& o : ops) {
            wallSeg(alongX, cur, o.a0, p0, p1, y0, y1, m);
            wallSeg(alongX, o.a0, o.a1, p0, p1, y0, o.y0, m);   // below the opening
            wallSeg(alongX, o.a0, o.a1, p0, p1, o.y1, y1, m);   // above the opening
            cur = o.a1;
        }
        wallSeg(alongX, cur, a1, p0, p1, y0, y1, m);
    }
    // A 0.2 m thick wall made of two 0.1 m skins so each face can have its own material AND lighting zone
    // (the outside of a wall gets sunlight, the inside does not).
    void wallSkins(bool alongX, float a0, float a1, float pm, float y0, float y1, std::vector<Opening> ops,
                   Zone zNeg, const Mat& mNeg, Zone zPos, const Mat& mPos)
    {
        zone(zNeg); wall(alongX, a0, a1, pm - 0.1f, pm, y0, y1, mNeg, ops);
        zone(zPos); wall(alongX, a0, a1, pm, pm + 0.1f, y0, y1, mPos, ops);
    }

    // glass pane (thin box) in an opening, plus an outer sill
    void window(bool alongX, float a0, float a1, float pm, float y0, float y1, float vRepeat = 1.0f)
    {
        zone(Zone::Outdoor);
        Mat g = mGlass; g.uv = glm::vec2(1.0f, vRepeat);
        if (alongX) {
            box(V3(a0, y0, pm - 0.03f), V3(a1, y1, pm + 0.03f), g);
            box(V3(a0 - 0.05f, y0 - 0.05f, pm - 0.16f), V3(a1 + 0.05f, y0, pm + 0.16f), mConcrete);
        } else {
            box(V3(pm - 0.03f, y0, a0), V3(pm + 0.03f, y1, a1), g);
            box(V3(pm - 0.16f, y0 - 0.05f, a0 - 0.05f), V3(pm + 0.16f, y0, a1 + 0.05f), mConcrete);
        }
    }
    void fakeWindow(bool alongX, float a0, float a1, float p, float y0, float y1, float sign)
    {
        zone(Zone::Outdoor);
        float c = p + 0.02f * sign;   // sign = +1 sticks out toward +p, -1 toward -p
        if (alongX) boxC(V3((a0 + a1) * 0.5f, (y0 + y1) * 0.5f, c), V3(a1 - a0, y1 - y0, 0.04f), decal(mWinDark));
        else        boxC(V3(c, (y0 + y1) * 0.5f, (a0 + a1) * 0.5f), V3(0.04f, y1 - y0, a1 - a0), decal(mWinDark));
    }

    void closedDoor(bool alongX, float a0, float a1, float pm)
    {
        const float t = 0.04f, d = 0.11f;
        if (alongX) {
            box(V3(a0 + 0.05f, 0.0f, pm - t), V3(a1 - 0.05f, 2.15f, pm + t), mDoor);
            box(V3(a0, 0.0f, pm - d), V3(a0 + 0.05f, 2.2f, pm + d), mFrame);
            box(V3(a1 - 0.05f, 0.0f, pm - d), V3(a1, 2.2f, pm + d), mFrame);
            box(V3(a0, 2.15f, pm - d), V3(a1, 2.2f, pm + d), mFrame);
            box(V3(a1 - 0.25f, 0.97f, pm - 0.09f), V3(a1 - 0.12f, 1.03f, pm + 0.09f), mMetal);
        } else {
            box(V3(pm - t, 0.0f, a0 + 0.05f), V3(pm + t, 2.15f, a1 - 0.05f), mDoor);
            box(V3(pm - d, 0.0f, a0), V3(pm + d, 2.2f, a0 + 0.05f), mFrame);
            box(V3(pm - d, 0.0f, a1 - 0.05f), V3(pm + d, 2.2f, a1), mFrame);
            box(V3(pm - d, 2.15f, a0), V3(pm + d, 2.2f, a1), mFrame);
            box(V3(pm - 0.09f, 0.97f, a1 - 0.25f), V3(pm + 0.09f, 1.03f, a1 - 0.12f), mMetal);
        }
    }
    // door leaf swung open: hinge at (hx,hz), angle in degrees measured in the XZ plane (0 = +X, 90 = +Z)
    void openDoor(float hx, float hz, float angle)
    {
        float r = glm::radians(angle), dx = std::cos(r), dz = std::sin(r);
        boxC(V3(hx + dx * 0.56f, 1.075f, hz + dz * 0.56f), V3(1.12f, 2.15f, 0.06f), mDoor, V3(0.0f, -angle, 0.0f));
        boxC(V3(hx + dx * 1.0f, 1.0f, hz + dz * 1.0f), V3(0.1f, 0.06f, 0.12f), mMetal, V3(0.0f, -angle, 0.0f));
    }

    // ---- signs and notices (use the mesh's own UVs, so the text appears once, upright)
    const Texture* signTex(const std::string& text, int style)
    {
        std::string key = "sign" + std::to_string(style) + text;
        auto it = m_tex.find(key);
        if (it == m_tex.end()) {
            V3 fg(1.0f), bg(0.1f);
            switch (style) {
            case 0: fg = V3(1.0f);            bg = V3(0.04f, 0.50f, 0.20f); break;   // green EXIT
            case 1: fg = V3(0.92f);           bg = V3(0.10f, 0.20f, 0.35f); break;   // blue room plate
            case 2: fg = V3(1.0f);            bg = V3(0.75f, 0.08f, 0.08f); break;   // red STOP
            case 3: fg = V3(0.05f);           bg = V3(0.95f, 0.78f, 0.10f); break;   // yellow warning
            case 4: fg = V3(0.95f, 0.92f, 0.8f); bg = V3(0.05f, 0.25f, 0.15f); break; // university green
            case 5: fg = V3(1.0f);            bg = V3(0.65f, 0.05f, 0.05f); break;   // red alert
            }
            it = m_tex.emplace(key, ProcTex::sign(text, fg, bg)).first;
        }
        return &it->second;
    }
    // facing +/-Z (hung on a wall that runs along X)
    void signZ(V3 c, float h, const std::string& text, int style, float glow = 0.0f, float depth = 0.04f)
    {
        Mat m = decal(uvMat(signTex(text, style), 1.0f, 1.0f, V3(1.0f), 8.0f, 0.0f)); m.emissive = glow;
        boxC(c, V3(h * ProcTex::signAspect(text), h, depth), m);
    }
    // facing +/-X (hung on a wall that runs along Z)
    void signX(V3 c, float h, const std::string& text, int style, float glow = 0.0f, float depth = 0.04f)
    {
        Mat m = decal(uvMat(signTex(text, style), 1.0f, 1.0f, V3(1.0f), 8.0f, 0.0f)); m.emissive = glow;
        boxC(c, V3(depth, h, h * ProcTex::signAspect(text)), m);
    }
    void noticeZ(V3 c, float w, float h, float shade, V3 rot = V3(0.0f))
    {
        boxC(c, V3(w, h, 0.012f), decal(uvMat(T("notice"), 1.0f, 1.0f, V3(shade, shade, shade * 0.95f), 4.0f, 0.0f)), rot);
    }
    void noticeX(V3 c, float w, float h, float shade)
    {
        boxC(c, V3(0.012f, h, w), decal(uvMat(T("notice"), 1.0f, 1.0f, V3(shade, shade, shade * 0.95f), 4.0f, 0.0f)));
    }
    void paper(float x, float z)   // a sheet lying on the floor
    {
        float yaw = rnd(m_seed) * 360.0f;
        boxC(V3(x, 0.004f, z), V3(0.21f, 0.008f, 0.30f), decal(uvMat(T("notice"), 1.0f, 1.0f, V3(0.85f + 0.15f * rnd(m_seed)), 4.0f, 0.0f)), V3(0.0f, yaw, 0.0f));
    }

    // ---- furniture
    void desk(float x, float z, float yaw);
    void teacherTable(float x, float z);
    void fan(float x, float z);
    void fallenChair(float x, float z, float yaw);
    void bench(float x, float z, float yaw);
    void tree(float x, float z, float yaw);
    void streetLight(float x, float z, float yaw, V3 color, int flicker);
    void car(float x, float z, float yaw, V3 paint);
    void bus(float x, float z, float yaw);
    void cng(float x, float z, float yaw);
    void rickshaw(float x, float z, float yaw);
    void railing(bool alongX, float a0, float a1, float p, float baseY, float h);
    void barrier(float x, float z, float yaw);
    void ammoCrate(V3 floorPos, float yaw, int index);
    void apartment(float x0, float x1, float z0, float z1, float h, V3 tint);

    // ---- areas
    void buildGround();
    void buildClassroom();
    void buildCorridor();
    void buildStairs();
    void buildFacade();
    void buildCourtyard();
    void buildGate();
    void buildRoad();
};

// ====================================================================== textures + materials
void Builder::loadTextures()
{
    add("brick",    Texture::createBrick());
    add("concrete", Texture::createConcrete());
    add("wood",     Texture::createWoodFloor());
    add("grass",    Texture::createGrass());
    add("board",    Texture::createBlackboard());
    add("plaster",  ProcTex::plaster());
    add("tile",     ProcTex::floorTile());
    add("asphalt",  ProcTex::asphalt());
    add("paving",   ProcTex::paving());
    add("metal",    ProcTex::metal());
    add("foliage",  ProcTex::foliage());
    add("door",     ProcTex::door());
    add("glass",    ProcTex::windowGlass());
    add("windark",  ProcTex::windowDark());
    add("facade",   ProcTex::facade());
    add("notice",   ProcTex::notice());
    add("hazard",   ProcTex::hazard());
    add("bars",     ProcTex::gateBars());
    add("flag",     ProcTex::flag());
}

void Builder::makeMats()
{
    mWallRoom  = worldMat(T("plaster"), 0.30f, V3(1.00f, 0.93f, 0.78f));
    mWallHall  = worldMat(T("plaster"), 0.30f, V3(0.72f, 0.78f, 0.74f));
    mWallOut   = worldMat(T("plaster"), 0.25f, V3(0.95f, 0.78f, 0.55f));
    mConcrete  = worldMat(T("concrete"), 0.40f, V3(1.0f));
    mConcDark  = worldMat(T("concrete"), 0.25f, V3(0.55f));
    mWood      = worldMat(T("wood"), 1.0f, V3(1.0f, 0.9f, 0.8f), 32.0f, 0.2f);
    mWoodDark  = worldMat(T("wood"), 1.0f, V3(0.45f, 0.35f, 0.30f), 16.0f, 0.1f);
    mWoodFloor = worldMat(T("wood"), 0.5f, V3(1.0f), 32.0f, 0.2f);
    mDeskTop   = worldMat(T("wood"), 1.5f, V3(1.1f, 0.95f, 0.8f), 32.0f, 0.25f);
    mChair     = worldMat(T("metal"), 1.0f, V3(0.30f, 0.45f, 0.75f), 48.0f, 0.4f);
    mTile      = worldMat(T("tile"), 0.5f, V3(1.0f), 48.0f, 0.3f);
    mBrick     = worldMat(T("brick"), 0.5f);
    mGrass     = worldMat(T("grass"), 0.5f);
    mAsphalt   = worldMat(T("asphalt"), 0.25f, V3(1.0f), 8.0f, 0.05f);
    mPaving    = worldMat(T("paving"), 0.5f);
    mMetal     = worldMat(T("metal"), 1.0f, V3(0.75f), 48.0f, 0.4f);
    mMetalDark = worldMat(T("metal"), 1.0f, V3(0.28f), 48.0f, 0.4f);
    mFoliage   = worldMat(T("foliage"), 0.7f, V3(0.85f, 1.0f, 0.8f), 4.0f, 0.0f);
    mDoor      = uvMat(T("door"), 1.0f, 1.0f, V3(1.0f), 16.0f, 0.1f);
    mFrame     = worldMat(T("wood"), 1.0f, V3(0.35f, 0.22f, 0.12f));
    mPaper     = uvMat(T("notice"), 1.0f, 1.0f);
    mGlass     = uvMat(T("glass"), 1.0f, 1.0f, V3(1.0f), 96.0f, 0.6f); mGlass.mode = 2;
    mWinDark   = uvMat(T("windark"), 1.0f, 1.0f, V3(1.0f), 96.0f, 0.5f);
    mStair     = worldMat(T("concrete"), 0.5f, V3(0.80f, 0.78f, 0.74f));
    mBoard     = worldMat(T("board"), 0.5f, V3(1.0f), 8.0f, 0.0f);
    mFacade    = worldMat(T("facade"), 0.28f, V3(1.0f));
    mGate      = worldMat(T("bars"), 1.0f, V3(0.35f, 0.45f, 0.40f), 48.0f, 0.4f); mGate.mode = 1;
    mHazard    = uvMat(T("hazard"), 4.0f, 1.0f);
    mFlag      = uvMat(T("flag"), 1.0f, 1.0f, V3(1.0f), 4.0f, 0.0f);
}

// ====================================================================== furniture & props
void Builder::desk(float x, float z, float yaw)
{
    Frame f{V3(x, 0.0f, z), yaw};
    part(f, V3(0, 0.725f, 0),      V3(1.1f, 0.05f, 0.6f),  mDeskTop);                 // top
    part(f, V3(-0.5f, 0.35f, 0),   V3(0.04f, 0.7f, 0.5f),  mMetalDark);              // side frames
    part(f, V3( 0.5f, 0.35f, 0),   V3(0.04f, 0.7f, 0.5f),  mMetalDark);
    part(f, V3(0, 0.40f, 0),       V3(0.95f, 0.03f, 0.5f), mDeskTop);                 // book shelf
    part(f, V3(0, 0.45f, 0.65f),   V3(0.42f, 0.04f, 0.42f), mChair);                  // chair seat
    part(f, V3(0, 0.72f, 0.85f),   V3(0.42f, 0.30f, 0.03f), mChair);                  // chair back
    part(f, V3(-0.18f, 0.22f, 0.65f), V3(0.03f, 0.44f, 0.36f), mMetalDark);           // chair legs
    part(f, V3( 0.18f, 0.22f, 0.65f), V3(0.03f, 0.44f, 0.36f), mMetalDark);
}

void Builder::teacherTable(float x, float z)
{
    Frame f{V3(x, 0.0f, z), 0.0f};
    part(f, V3(0, 0.78f, 0),       V3(1.7f, 0.05f, 0.75f), mDeskTop);
    part(f, V3(-0.75f, 0.38f, 0),  V3(0.08f, 0.76f, 0.7f), mWoodDark);
    part(f, V3( 0.75f, 0.38f, 0),  V3(0.08f, 0.76f, 0.7f), mWoodDark);
    part(f, V3(0, 0.45f, 0.30f),   V3(1.4f, 0.6f, 0.04f),  mWoodDark);                // front panel
    // computer monitor showing the emergency alert (this is the story prop of the opening scene)
    part(f, V3(0.3f, 0.83f, -0.05f), V3(0.25f, 0.05f, 0.18f), mMetalDark);
    Mat screen = uvMat(signTex("ALERT", 5), 1.0f, 1.0f, V3(1.0f), 8.0f, 0.0f); screen.emissive = 0.9f;
    part(f, V3(0.3f, 1.05f, -0.05f), V3(0.55f, 0.36f, 0.04f), mMetalDark);
    part(f, V3(0.3f, 1.05f, -0.025f), V3(0.50f, 0.31f, 0.01f), screen);
    // teacher's chair
    part(f, V3(0, 0.45f, 0.85f),   V3(0.46f, 0.05f, 0.46f), mWoodDark);
    part(f, V3(0, 0.80f, 1.06f),   V3(0.46f, 0.6f, 0.04f),  mWoodDark);
}

void Builder::fan(float x, float z)
{
    Frame f{V3(x, 0.0f, z), 15.0f};
    part(f, V3(0, 3.05f, 0), V3(0.03f, 0.3f, 0.03f), mMetalDark);   // rod from the ceiling
    part(f, V3(0, 2.88f, 0), V3(0.2f, 0.1f, 0.2f), mMetalDark);     // motor
    for (int i = 0; i < 3; ++i) {                                   // three blades (the power is out, so they are still)
        Frame b{V3(x, 0.0f, z), 15.0f + i * 120.0f};
        part(b, V3(0.45f, 2.88f, 0), V3(0.7f, 0.02f, 0.14f), mMetal, V3(0, 0, -3.0f));
    }
}

void Builder::fallenChair(float x, float z, float yaw)
{
    Frame f{V3(x, 0.0f, z), yaw};
    part(f, V3(0, 0.25f, 0),        V3(0.42f, 0.42f, 0.04f), mChair);            // seat standing on its edge
    part(f, V3(0, 0.02f, 0.18f),    V3(0.42f, 0.03f, 0.30f), mChair);            // back lying on the floor
    part(f, V3(-0.18f, 0.35f, -0.22f), V3(0.03f, 0.03f, 0.44f), mMetalDark);     // legs pointing sideways
    part(f, V3( 0.18f, 0.35f, -0.22f), V3(0.03f, 0.03f, 0.44f), mMetalDark);
}

void Builder::bench(float x, float z, float yaw)
{
    // front (where you sit facing) is local -Z
    Frame f{V3(x, 0.0f, z), yaw};
    part(f, V3(0, 0.45f, 0),       V3(1.6f, 0.06f, 0.45f), mWood);
    part(f, V3(0, 0.75f, 0.2f),    V3(1.6f, 0.40f, 0.05f), mWood);
    part(f, V3(-0.7f, 0.22f, 0),   V3(0.08f, 0.44f, 0.40f), mConcDark);
    part(f, V3( 0.7f, 0.22f, 0),   V3(0.08f, 0.44f, 0.40f), mConcDark);
}

void Builder::tree(float x, float z, float yaw)
{
    // trunk + four overlapping rotated "leaf blobs": a cheap but organic-looking low-poly tree
    Frame f{V3(x, 0.0f, z), yaw};
    part(f, V3(0, 1.3f, 0),          V3(0.32f, 2.6f, 0.32f), mWoodDark);
    part(f, V3(0, 3.0f, 0),          V3(2.2f, 1.4f, 2.2f), mFoliage);
    part(f, V3(0, 3.5f, 0),          V3(1.7f, 1.4f, 1.7f), mFoliage, V3(0, 35, 0));
    part(f, V3(0.5f, 2.6f, 0.3f),    V3(1.4f, 1.0f, 1.4f), mFoliage, V3(0, 60, 0));
    part(f, V3(-0.5f, 2.7f, -0.2f),  V3(1.3f, 1.0f, 1.3f), mFoliage, V3(0, 15, 0));
}

void Builder::streetLight(float x, float z, float yaw, V3 color, int flicker)
{
    Frame f{V3(x, 0.0f, z), yaw};
    int li = addLight(V3(x, 4.0f, z) + V3(std::cos(glm::radians(yaw)) * 0.95f, 0.0f, -std::sin(glm::radians(yaw)) * 0.95f),
                      color, 11.0f, flicker);
    part(f, V3(0, 0.2f, 0),        V3(0.25f, 0.4f, 0.25f), mConcDark);
    part(f, V3(0, 2.1f, 0),        V3(0.09f, 4.2f, 0.09f), mMetalDark);
    part(f, V3(0.45f, 4.15f, 0),   V3(0.95f, 0.06f, 0.06f), mMetalDark);
    part(f, V3(0.95f, 4.08f, 0),   V3(0.50f, 0.10f, 0.22f), fixture(li, V3(1.0f, 0.8f, 0.5f), 1.6f));
}

void Builder::car(float x, float z, float yaw, V3 paint)
{
    Frame f{V3(x, 0.0f, z), yaw};
    Mat body = worldMat(T("metal"), 1.0f, paint, 64.0f, 0.5f);
    part(f, V3(0, 0.55f, 0),       V3(4.2f, 0.7f, 1.8f), body);
    part(f, V3(-0.2f, 1.12f, 0),   V3(2.3f, 0.5f, 1.56f), mWinDark);
    part(f, V3(-0.2f, 1.43f, 0),   V3(2.1f, 0.06f, 1.5f), body);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            part(f, V3(1.3f * sx, 0.3f, 0.85f * sz), V3(0.6f, 0.6f, 0.22f), mMetalDark);
}

void Builder::bus(float x, float z, float yaw)
{
    Frame f{V3(x, 0.0f, z), yaw};
    Mat body = worldMat(T("metal"), 1.0f, V3(0.75f, 0.15f, 0.10f), 32.0f, 0.3f);
    Mat band = worldMat(T("metal"), 1.0f, V3(0.9f, 0.85f, 0.7f), 32.0f, 0.3f);
    part(f, V3(0, 1.3f, 0),            V3(9.0f, 2.2f, 2.5f), body);
    part(f, V3(0, 0.45f, 0),           V3(9.0f, 0.3f, 2.5f), band);
    part(f, V3(0, 1.95f, 1.26f),       V3(8.4f, 0.8f, 0.04f), uvMat(T("windark"), 6.0f, 1.0f, V3(1.0f), 96.0f, 0.5f));
    part(f, V3(0, 1.95f, -1.26f),      V3(8.4f, 0.8f, 0.04f), uvMat(T("windark"), 6.0f, 1.0f, V3(1.0f), 96.0f, 0.5f));
    part(f, V3(0, 2.45f, 0),           V3(8.8f, 0.1f, 2.4f), band);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            part(f, V3(3.0f * sx, 0.5f, 1.2f * sz), V3(1.0f, 1.0f, 0.3f), mMetalDark);
}

void Builder::cng(float x, float z, float yaw)   // CNG auto-rickshaw, a very Dhaka sight
{
    Frame f{V3(x, 0.0f, z), yaw};
    Mat green = worldMat(T("metal"), 1.0f, V3(0.15f, 0.55f, 0.25f), 48.0f, 0.4f);
    part(f, V3(0, 0.8f, 0),          V3(2.2f, 1.0f, 1.2f), green);
    part(f, V3(0, 1.45f, 0),         V3(2.3f, 0.06f, 1.3f), green);
    part(f, V3(1.0f, 1.0f, 0),       V3(0.06f, 0.6f, 1.0f), mWinDark);
    part(f, V3(0.9f, 0.25f, 0),      V3(0.5f, 0.5f, 0.12f), mMetalDark);
    part(f, V3(-0.7f, 0.25f, 0.62f), V3(0.5f, 0.5f, 0.12f), mMetalDark);
    part(f, V3(-0.7f, 0.25f, -0.62f), V3(0.5f, 0.5f, 0.12f), mMetalDark);
}

void Builder::rickshaw(float x, float z, float yaw)
{
    Frame f{V3(x, 0.0f, z), yaw};
    Mat pink = worldMat(T("metal"), 1.0f, V3(0.85f, 0.25f, 0.45f), 32.0f, 0.3f);
    Mat teal = worldMat(T("metal"), 1.0f, V3(0.15f, 0.6f, 0.65f), 32.0f, 0.3f);
    part(f, V3(0, 0.5f, 0),          V3(1.6f, 0.08f, 0.9f), teal);
    part(f, V3(-0.55f, 0.85f, 0),    V3(0.1f, 0.65f, 0.85f), teal);
    part(f, V3(-0.15f, 1.35f, 0),    V3(1.2f, 0.05f, 1.0f), pink, V3(0, 0, 15));
    part(f, V3(-0.6f, 0.4f, 0.5f),   V3(0.8f, 0.8f, 0.05f), mMetalDark);
    part(f, V3(-0.6f, 0.4f, -0.5f),  V3(0.8f, 0.8f, 0.05f), mMetalDark);
    part(f, V3(0.85f, 0.3f, 0),      V3(0.6f, 0.6f, 0.05f), mMetalDark);
    part(f, V3(1.0f, 0.75f, 0),      V3(0.05f, 0.9f, 0.05f), mMetalDark);
}

void Builder::railing(bool alongX, float a0, float a1, float p, float baseY, float h)
{
    int n = std::max(2, (int)std::ceil((a1 - a0) / 0.7f) + 1);
    for (int i = 0; i < n; ++i) {
        float a = a0 + (a1 - a0) * i / (n - 1);
        if (alongX) boxC(V3(a, baseY + h * 0.5f, p), V3(0.04f, h, 0.04f), mMetalDark);
        else        boxC(V3(p, baseY + h * 0.5f, a), V3(0.04f, h, 0.04f), mMetalDark);
    }
    float len = a1 - a0, mid = (a0 + a1) * 0.5f;
    if (alongX) { boxC(V3(mid, baseY + h, p), V3(len, 0.05f, 0.06f), mMetal); boxC(V3(mid, baseY + h * 0.5f, p), V3(len, 0.03f, 0.03f), mMetalDark); }
    else        { boxC(V3(p, baseY + h, mid), V3(0.06f, 0.05f, len), mMetal); boxC(V3(p, baseY + h * 0.5f, mid), V3(0.03f, 0.03f, len), mMetalDark); }
}

void Builder::barrier(float x, float z, float yaw)
{
    Frame f{V3(x, 0.0f, z), yaw};
    part(f, V3(0, 0.85f, 0),       V3(1.8f, 0.25f, 0.06f), mHazard);
    part(f, V3(0, 0.55f, 0),       V3(1.8f, 0.2f, 0.06f), mHazard);
    part(f, V3(-0.8f, 0.4f, 0),    V3(0.06f, 0.8f, 0.5f), mMetalDark);
    part(f, V3( 0.8f, 0.4f, 0),    V3(0.06f, 0.8f, 0.5f), mMetalDark);
}

// An ammunition crate with a small floating golden bullet so it is easy to spot. Tag 8+index lets the
// mission hide it once collected. Crates are low and not solid (you simply walk over them).
void Builder::ammoCrate(V3 floorPos, float yaw, int index)
{
    Mat body = decal(worldMat(T("metal"), 1.0f, V3(0.22f, 0.30f, 0.14f), 32.0f, 0.3f));
    Mat lid  = decal(worldMat(T("metal"), 1.0f, V3(0.32f, 0.40f, 0.19f), 32.0f, 0.3f));
    Mat gold = worldMat(T("concrete"), 1.0f, V3(1.0f, 0.80f, 0.20f), 8.0f, 0.0f);
    gold.emissive = 1.6f; gold = decal(gold);
    Frame f{floorPos, yaw};
    m_tag = 8 + index;
    part(f, V3(0.0f, 0.10f, 0.0f),  V3(0.36f, 0.20f, 0.24f), body);
    part(f, V3(0.0f, 0.215f, 0.0f), V3(0.38f, 0.03f, 0.26f), lid);
    part(f, V3(0.0f, 0.236f, 0.0f), V3(0.30f, 0.01f, 0.05f), gold);              // yellow stripe on the lid
    part(f, V3(0.0f, 0.62f, 0.0f),  V3(0.05f, 0.16f, 0.05f), gold);              // floating bullet icon
    part(f, V3(0.0f, 0.72f, 0.0f),  V3(0.05f, 0.04f, 0.05f), gold);
    m_tag = 0;
}

void Builder::apartment(float x0, float x1, float z0, float z1, float h, V3 tint)
{
    zone(Zone::Outdoor);
    Mat m = mFacade; m.tint = tint;
    box(V3(x0, 0.0f, z0), V3(x1, h, z1), m);
    box(V3(x0 - 0.2f, h, z0 - 0.2f), V3(x1 + 0.2f, h + 0.3f, z1 + 0.2f), mConcDark);          // roof edge
    box(V3((x0 + x1) * 0.5f - 0.8f, h + 0.3f, (z0 + z1) * 0.5f - 0.8f),
        V3((x0 + x1) * 0.5f + 0.8f, h + 1.9f, (z0 + z1) * 0.5f + 0.8f), mMetal);              // rooftop water tank
}

// ====================================================================== the map
void Builder::buildGround()
{
    zone(Zone::Outdoor);
    box(V3(-120, -1.0f, -120), V3(120, -0.12f, 120), mConcDark);   // base ground
    box(V3(-26, -0.6f, -30), V3(26, -0.1f, 34), mGrass);            // campus lawn
}

// ---------------------------------------------------------------- Area 1: classroom (x -5..5, z -4..4)
void Builder::buildClassroom()
{
    Mat ceiling = mConcrete; ceiling.tint = V3(0.9f, 0.9f, 0.85f);

    zone(Zone::Room);
    box(V3(-5.2f, -0.2f, -4.2f), V3(5.2f, 0.0f, 4.2f), mWoodFloor);
    box(V3(-5.2f, 3.2f, -4.2f), V3(5.2f, 3.4f, 4.0f), ceiling);

    wallSkins(true, -5.2f, 5.2f, -4.1f, 0, 3.2f, {}, Zone::Outdoor, mWallOut, Zone::Room, mWallRoom);        // blackboard wall
    std::vector<Opening> win = { {-3.5f, -1.7f, 1.0f, 2.4f}, {-0.9f, 0.9f, 1.0f, 2.4f}, {1.7f, 3.5f, 1.0f, 2.4f} };
    wallSkins(false, -4.0f, 4.0f, -5.1f, 0, 3.2f, win, Zone::Outdoor, mWallOut, Zone::Room, mWallRoom);     // window wall
    for (const Opening& o : win) window(false, o.a0, o.a1, -5.1f, o.y0, o.y1);
    wallSkins(false, -4.0f, 4.0f, 5.1f, 0, 3.2f, {}, Zone::Room, mWallRoom, Zone::Outdoor, mWallOut);        // east wall
    // wall shared with the corridor: classroom door + two closed doors to the neighbouring rooms
    wallSkins(true, -12.0f, 12.2f, 4.1f, 0, 3.2f,
              { {3.0f, 4.2f, 0, 2.2f}, {-9.4f, -8.2f, 0, 2.2f}, {8.2f, 9.4f, 0, 2.2f} },
              Zone::Room, mWallRoom, Zone::Hall, mWallHall);

    // the classroom door is open (someone ran out in a hurry)
    zone(Zone::Room);
    box(V3(2.95f, 0, 3.99f), V3(3.0f, 2.2f, 4.21f), mFrame);
    box(V3(4.2f, 0, 3.99f), V3(4.25f, 2.2f, 4.21f), mFrame);
    box(V3(3.0f, 2.15f, 3.99f), V3(4.2f, 2.2f, 4.21f), mFrame);
    openDoor(3.0f, 4.1f, 75.0f);

    // blackboard, chalk tray, notices, pin board
    box(V3(-3.0f, 1.0f, -4.0f), V3(3.0f, 2.4f, -3.93f), mBoard);
    box(V3(-3.1f, 0.95f, -4.0f), V3(3.1f, 1.0f, -3.9f), mWood);
    box(V3(-3.1f, 2.4f, -4.0f), V3(3.1f, 2.45f, -3.93f), mWood);
    noticeZ(V3(-4.0f, 1.7f, -3.99f), 0.40f, 0.55f, 0.95f);
    noticeZ(V3(4.0f, 1.6f, -3.99f), 0.45f, 0.60f, 0.85f);
    Mat cork = mWood; cork.tint = V3(0.75f, 0.52f, 0.30f);
    box(V3(4.96f, 1.0f, -3.0f), V3(5.0f, 2.2f, 3.0f), cork);
    const float nz[5] = {-2.5f, -1.5f, -0.3f, 1.0f, 2.3f};
    for (int i = 0; i < 5; ++i) noticeX(V3(4.953f, 1.5f + 0.35f * (i % 2), nz[i]), 0.35f, 0.45f, 0.8f + 0.04f * i);

    // furniture: 3 rows x 4 columns of desks with chairs, slightly disordered
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col) {
            if (row == 2 && col == 2) continue;                       // one desk missing
            desk(-3.3f + col * 2.2f, -1.8f + row * 1.7f, (rnd(m_seed) - 0.5f) * 14.0f);
        }
    teacherTable(0.0f, -3.4f);
    zone(Zone::Room);
    ammoCrate(V3(-4.3f, 0.0f, -3.2f), 20.0f, 0);                 // ammunition: front-left corner of the classroom
    fallenChair(1.0f, 1.0f, 40.0f);
    fallenChair(-4.4f, 0.8f, -70.0f);

    // ceiling: fans (power is out, so they are still) and two tube-light fixtures
    fan(-2.5f, 1.2f);
    fan(2.5f, 1.2f);
    for (int i = 0; i < 2; ++i) {
        float x = (i == 0) ? -2.5f : 2.5f;
        int li = addLight(V3(x, 2.9f, -1.0f), V3(1.0f, 0.9f, 0.7f) * 0.85f, 8.0f, 1);
        boxC(V3(x, 3.17f, -1.0f), V3(1.2f, 0.06f, 0.3f), fixture(li, V3(1.0f, 0.95f, 0.85f)));
    }

    // damage and dirt on the floor
    Mat rubble = mConcrete; rubble.tint = V3(0.7f);
    boxC(V3(-1.5f, 0.015f, 1.0f), V3(0.6f, 0.03f, 0.6f), rubble, V3(0, 20, 0));     // fallen ceiling tile
    boxC(V3(3.0f, 0.025f, -0.6f), V3(0.35f, 0.05f, 0.25f), rubble, V3(0, -35, 0));
    boxC(V3(-4.2f, 0.02f, 3.2f), V3(0.4f, 0.04f, 0.3f), rubble, V3(0, 50, 0));
    for (int i = 0; i < 12; ++i) paper(-4.3f + rnd(m_seed) * 8.6f, -3.0f + rnd(m_seed) * 6.0f);

    // room blocks next to the classroom (solid, seen from outside only)
    zone(Zone::Outdoor);
    box(V3(-12.2f, 0, -1.0f), V3(-7.0f, 3.4f, 4.0f), mWallOut);
    box(V3(7.0f, 0, -1.0f), V3(12.2f, 3.4f, 4.0f), mWallOut);
    for (int i = 0; i < 2; ++i) {
        fakeWindow(true, -11.4f + i * 2.2f, -10.2f + i * 2.2f, -1.0f, 1.0f, 2.4f, -1.0f);
        fakeWindow(true,  8.0f + i * 2.2f,   9.2f + i * 2.2f,  -1.0f, 1.0f, 2.4f, -1.0f);
    }
}

// ---------------------------------------------------------------- Area 2: corridor (x -12..12, z 4.2..7.2)
void Builder::buildCorridor()
{
    Mat ceiling = mConcrete; ceiling.tint = V3(0.78f);
    zone(Zone::Hall);
    box(V3(-19.2f, -0.2f, 4.2f), V3(12.2f, 0.0f, 7.4f), mTile);
    box(V3(-12.0f, 3.2f, 4.0f), V3(12.2f, 3.4f, 7.4f), ceiling);

    // south wall (faces the courtyard): windows + the exit door
    std::vector<Opening> sw = { {3.0f, 4.2f, 0, 2.2f} };
    const float wx[6][2] = { {-10.8f, -9.0f}, {-7.4f, -5.6f}, {-4.0f, -2.2f}, {-0.6f, 1.2f}, {5.4f, 7.2f}, {8.8f, 10.6f} };
    for (auto& w : wx) sw.push_back({ w[0], w[1], 1.0f, 2.5f });
    wallSkins(true, -12.0f, 12.2f, 7.3f, 0, 3.2f, sw, Zone::Hall, mWallHall, Zone::Outdoor, mWallOut);
    for (auto& w : wx) window(true, w[0], w[1], 7.3f, 1.0f, 2.5f);
    zone(Zone::Outdoor);
    box(V3(2.95f, 0, 7.19f), V3(3.0f, 2.2f, 7.41f), mFrame);
    box(V3(4.2f, 0, 7.19f), V3(4.25f, 2.2f, 7.41f), mFrame);
    box(V3(3.0f, 2.15f, 7.19f), V3(4.2f, 2.2f, 7.41f), mFrame);
    openDoor(3.0f, 7.3f, 100.0f);

    // east end: emergency exit double door (closed)
    wallSkins(false, 4.0f, 7.4f, 12.1f, 0, 3.2f, { {5.0f, 6.4f, 0, 2.2f} }, Zone::Hall, mWallHall, Zone::Outdoor, mWallOut);
    zone(Zone::Hall);
    closedDoor(false, 5.0f, 6.4f, 12.1f);
    signX(V3(11.975f, 2.5f, 5.7f), 0.25f, "EXIT", 0, 0.9f);
    addLight(V3(11.4f, 2.3f, 5.7f), V3(0.2f, 1.0f, 0.4f) * 0.35f, 4.0f, 0);          // the green glow of the emergency exit sign
    addLight(V3(3.75f, 2.2f, 7.0f), V3(1.0f, 0.55f, 0.25f) * 0.7f, 5.5f, 0);          // sunset light spilling in through the open exit door

    // doors, plates and signs
    closedDoor(true, -9.4f, -8.2f, 4.1f);
    closedDoor(true, 8.2f, 9.4f, 4.1f);
    signZ(V3(3.6f, 2.45f, 4.22f), 0.20f, "ROOM 101", 1);
    signZ(V3(-8.8f, 2.45f, 4.22f), 0.20f, "ROOM 102", 1);
    signZ(V3(8.8f, 2.45f, 4.22f), 0.20f, "ROOM 103", 1);
    signZ(V3(3.6f, 2.5f, 7.175f), 0.25f, "EXIT", 0, 0.9f);
    signZ(V3(-10.3f, 2.5f, 4.22f), 0.20f, "STAIRS", 1);

    // ceiling lights: one broken, one pulsing red emergency light
    struct L { float x; V3 col; int fl; V3 tint; };
    const L ls[4] = {
        {-8.0f, V3(0.75f, 0.85f, 1.0f) * 0.75f, 2, V3(0.8f, 0.9f, 1.0f)},
        {-1.0f, V3(0.75f, 0.85f, 1.0f) * 0.60f, 1, V3(0.8f, 0.9f, 1.0f)},
        { 6.0f, V3(1.0f, 0.20f, 0.12f) * 0.90f, 3, V3(1.0f, 0.25f, 0.15f)},
        {11.0f, V3(0.75f, 0.85f, 1.0f) * 0.60f, 0, V3(0.8f, 0.9f, 1.0f)},
    };
    for (const L& l : ls) {
        int li = addLight(V3(l.x, 3.0f, 5.7f), l.col, 7.0f, l.fl);
        boxC(V3(l.x, 3.17f, 5.7f), V3(1.2f, 0.06f, 0.25f), fixture(li, l.tint));
    }

    // notice boards on the classroom wall
    Mat cork = mWood; cork.tint = V3(0.75f, 0.52f, 0.30f);
    const float bx[2] = { -2.0f, 7.0f };
    for (int i = 0; i < 2; ++i) {
        box(V3(bx[i] - 0.8f, 1.1f, 4.2f), V3(bx[i] + 0.8f, 2.1f, 4.24f), cork);
        noticeZ(V3(bx[i] - 0.45f, 1.55f, 4.25f), 0.35f, 0.45f, 0.9f);
        noticeZ(V3(bx[i] + 0.05f, 1.65f, 4.25f), 0.40f, 0.50f, 0.8f, V3(0, 0, 4));
        noticeZ(V3(bx[i] + 0.50f, 1.50f, 4.25f), 0.30f, 0.40f, 0.95f);
    }
    // fire extinguisher, lockers, bench, scattered objects
    boxC(V3(-5.85f, 1.15f, 4.28f), V3(0.15f, 0.5f, 0.15f), worldMat(T("metal"), 1.0f, V3(0.85f, 0.1f, 0.08f), 64.0f, 0.5f));
    Mat locker = worldMat(T("metal"), 1.0f, V3(0.25f, 0.5f, 0.55f), 48.0f, 0.4f);
    for (int i = 0; i < 3; ++i) box(V3(-11.7f + i * 0.4f, 0, 4.2f), V3(-11.32f + i * 0.4f, 1.8f, 4.55f), locker);
    boxC(V3(-6.0f, 0.19f, 4.8f), V3(0.38f, 1.8f, 0.35f), locker, V3(0, 25, 90));        // toppled locker
    bench(-3.0f, 6.85f, 0.0f);
    zone(Zone::Hall);
    ammoCrate(V3(-9.2f, 0.0f, 6.85f), -10.0f, 1);                // ammunition: corridor, under the window near the stairs
    boxC(V3(1.0f, 0.175f, 6.2f), V3(0.35f, 0.5f, 0.35f), mMetalDark, V3(0, 0, 90));      // overturned bin
    fallenChair(8.0f, 4.9f, -30.0f);
    fallenChair(-4.5f, 4.9f, 70.0f);
    for (int i = 0; i < 20; ++i) paper(-11.0f + rnd(m_seed) * 22.0f, 4.6f + rnd(m_seed) * 2.4f);
}

// ---------------------------------------------------------------- staircase + upper hall
void Builder::buildStairs()
{
    zone(Zone::Hall);
    // landing (1.6 m up) and two flights of solid steps (rise 0.2, run 0.35)
    box(V3(-19.0f, 0, 4.2f), V3(-15.2f, 1.6f, 7.2f), mStair);
    for (int i = 0; i < 8; ++i) {           // flight A: from the corridor up to the landing, along the north half
        float x1 = -12.4f - 0.35f * i;
        box(V3(x1 - 0.35f, 0, 4.2f), V3(x1, 0.2f * (i + 1), 5.7f), mStair);
    }
    for (int j = 0; j < 9; ++j) {           // flight B: from the landing up to the upper floor, along the south half
        float x0 = -15.2f + 0.35f * j;
        float x1 = (j == 8) ? -12.0f : x0 + 0.35f;
        box(V3(x0, 0, 5.7f), V3(x1, 1.6f + 0.2f * (j + 1), 7.2f), mStair);
    }
    // centre railing: posts + sloped hand rails (slope = 0.2 / 0.35)
    const float slope = 0.2f / 0.35f, ang = glm::degrees(std::atan(slope));
    for (int i = 0; i < 8; ++i) {
        float xc = -12.4f - 0.35f * (i + 0.5f), top = 0.2f * (i + 1), ry = 0.9f + slope * (-12.4f - xc);
        boxC(V3(xc, (top + ry) * 0.5f, 5.7f), V3(0.04f, ry - top, 0.04f), mMetalDark);
    }
    for (int j = 0; j < 9; ++j) {
        float xc = -15.2f + 0.35f * (j + 0.5f), top = 1.6f + 0.2f * (j + 1), ry = 2.5f + slope * (xc + 15.2f);
        boxC(V3(xc, (top + ry) * 0.5f, 5.7f), V3(0.04f, ry - top, 0.04f), mMetalDark);
    }
    boxC(V3(-13.8f, 1.7f, 5.7f), V3(std::sqrt(2.8f * 2.8f + 1.6f * 1.6f), 0.05f, 0.06f), mMetal, V3(0, 0, -ang));
    boxC(V3(-13.625f, 3.4f, 5.7f), V3(std::sqrt(3.15f * 3.15f + 1.8f * 1.8f), 0.05f, 0.06f), mMetal, V3(0, 0, ang));
    railing(false, 4.2f, 5.7f, -11.97f, 3.4f, 1.0f);      // guard rail on the upper floor, above flight A's void

    // stairwell shell (two storeys high)
    wallSkins(false, 4.2f, 7.2f, -19.1f, 0, 6.8f, {}, Zone::Outdoor, mWallOut, Zone::Hall, mWallHall);
    wallSkins(true, -19.2f, -12.0f, 4.1f, 0, 6.8f, {}, Zone::Outdoor, mWallOut, Zone::Hall, mWallHall);
    wallSkins(true, -19.2f, -12.0f, 7.3f, 0, 6.8f, { {-18.2f, -16.2f, 2.4f, 6.2f} }, Zone::Hall, mWallHall, Zone::Outdoor, mWallOut);
    window(true, -18.2f, -16.2f, 7.3f, 2.4f, 6.2f, 2.0f);
    Mat ceiling = mConcrete; ceiling.tint = V3(0.78f);
    zone(Zone::Hall);
    box(V3(-19.2f, 6.6f, 4.0f), V3(-6.0f, 6.8f, 7.4f), ceiling);

    // upper hall (x -12..-6): a dead end, blocked by a barricade
    wallSkins(true, -12.0f, -6.0f, 4.1f, 3.4f, 6.8f, {}, Zone::Outdoor, mWallOut, Zone::Hall, mWallHall);
    wallSkins(true, -12.0f, -6.0f, 7.3f, 3.4f, 6.8f, { {-11.0f, -9.4f, 4.4f, 5.8f}, {-8.4f, -6.8f, 4.4f, 5.8f} },
              Zone::Hall, mWallHall, Zone::Outdoor, mWallOut);
    window(true, -11.0f, -9.4f, 7.3f, 4.4f, 5.8f);
    window(true, -8.4f, -6.8f, 7.3f, 4.4f, 5.8f);
    zone(Zone::Hall);
    box(V3(-6.1f, 3.4f, 4.2f), V3(-6.0f, 6.6f, 7.2f), mWallHall);
    signX(V3(-6.115f, 4.9f, 5.7f), 0.30f, "CLOSED", 3);
    boxC(V3(-6.7f, 3.8f, 5.2f), V3(0.7f, 0.8f, 1.4f), mDeskTop, V3(0, 10, 0));
    boxC(V3(-6.6f, 4.6f, 6.2f), V3(0.6f, 0.5f, 1.2f), mDeskTop, V3(0, -15, 20));
    boxC(V3(-6.9f, 3.7f, 6.9f), V3(0.5f, 0.6f, 0.5f), mWoodDark, V3(0, 30, 0));
    boxC(V3(-6.5f, 4.4f, 4.6f), V3(0.1f, 1.6f, 0.4f), mWoodDark, V3(0, 0, 25));

    // the access card: on a small table just past the top of the stairs, with a glowing beacon
    box(V3(-11.2f, 3.4f, 4.7f), V3(-10.4f, 4.15f, 5.5f), mDeskTop);
    Mat cardMat = worldMat(T("concrete"), 1.0f, V3(1.0f, 0.85f, 0.2f), 8.0f, 0.0f);
    cardMat.emissive = 1.8f; cardMat = decal(cardMat);
    m_cardLight = addLight(sites::kCard + V3(0.0f, 0.5f, 0.0f), V3(1.0f, 0.85f, 0.3f) * 0.8f, 5.0f, 1);
    cardMat.light = m_cardLight;
    m_tag = 1;
    boxC(sites::kCard, V3(0.30f, 0.02f, 0.20f), cardMat);                       // the card
    boxC(sites::kCard + V3(0.0f, 0.95f, 0.0f), V3(0.05f, 1.8f, 0.05f), cardMat);  // a thin beacon so it can be spotted
    m_tag = 0;

    zone(Zone::Hall);
    ammoCrate(V3(-18.4f, 1.6f, 4.6f), 30.0f, 2);                 // ammunition: staircase landing

    // lights: amber lamp on the landing wall, flickering lamp in the upper hall
    int l1 = addLight(V3(-18.2f, 3.2f, 5.7f), V3(1.0f, 0.75f, 0.45f) * 0.8f, 9.0f, 1);
    box(V3(-19.0f, 2.8f, 5.4f), V3(-18.9f, 3.2f, 6.0f), fixture(l1, V3(1.0f, 0.8f, 0.5f)));
    // a red emergency light pulsing over the landing
    int lr = addLight(V3(-16.5f, 3.4f, 6.5f), V3(1.0f, 0.10f, 0.06f) * 0.85f, 7.5f, 3);
    boxC(V3(-16.5f, 3.6f, 7.15f), V3(0.30f, 0.12f, 0.08f), fixture(lr, V3(1.0f, 0.15f, 0.1f), 2.0f));
    int l2 = addLight(V3(-9.0f, 6.1f, 5.7f), V3(0.7f, 0.85f, 1.0f) * 0.7f, 8.0f, 2);
    boxC(V3(-9.0f, 6.57f, 5.7f), V3(1.2f, 0.06f, 0.25f), fixture(l2, V3(0.8f, 0.9f, 1.0f)));
}

// ---------------------------------------------------------------- building exterior seen from the courtyard
void Builder::buildFacade()
{
    zone(Zone::Outdoor);
    box(V3(-6.0f, 3.4f, 4.0f), V3(12.2f, 6.8f, 7.4f), mWallOut);                  // second floor block
    box(V3(-19.4f, 6.8f, 3.9f), V3(12.4f, 7.05f, 7.55f), mConcrete);               // roof cap
    box(V3(-19.4f, 3.4f, 7.4f), V3(12.4f, 3.55f, 7.55f), mConcrete);               // belt course
    for (int i = 0; i < 6; ++i) fakeWindow(true, -4.8f + 3.0f * i, -3.2f + 3.0f * i, 7.4f, 4.4f, 5.9f, 1.0f);
    Mat tank = mMetal; tank.tint = V3(0.4f, 0.5f, 0.7f);
    box(V3(7.0f, 7.05f, 5.0f), V3(8.4f, 8.4f, 6.4f), tank);                        // rooftop water tank
    box(V3(-3.0f, 7.05f, 5.48f), V3(-2.96f, 10.0f, 5.52f), mMetalDark);            // antenna

    // entrance canopy over the exit door
    box(V3(2.4f, 2.55f, 7.4f), V3(4.8f, 2.7f, 9.2f), mConcrete);
    box(V3(2.4f, 0, 9.0f), V3(2.6f, 2.55f, 9.2f), mConcrete);
    box(V3(4.6f, 0, 9.0f), V3(4.8f, 2.55f, 9.2f), mConcrete);

    // two more campus buildings that close in the courtyard
    Mat a = mFacade; a.tint = V3(1.0f, 0.88f, 0.75f);
    Mat b = mFacade; b.tint = V3(0.85f, 0.9f, 1.0f);
    box(V3(-25.5f, 0, 20.0f), V3(-17.0f, 7.5f, 29.0f), a);
    box(V3(-25.7f, 7.5f, 19.8f), V3(-16.8f, 7.75f, 29.2f), mConcrete);
    box(V3(17.0f, 0, 14.0f), V3(25.5f, 0, 22.0f) + V3(0, 6.5f, 0), b);
    box(V3(16.8f, 6.5f, 13.8f), V3(25.7f, 6.75f, 22.2f), mConcrete);
}

// ---------------------------------------------------------------- Area 3: courtyard
void Builder::buildCourtyard()
{
    zone(Zone::Outdoor);
    box(V3(2.4f, -0.1f, 7.4f), V3(4.8f, -0.02f, 34.0f), mPaving);          // main walkway
    box(V3(-19.2f, -0.1f, 7.4f), V3(12.4f, -0.02f, 8.8f), mPaving);        // apron in front of the building
    box(V3(-26.0f, -0.1f, 17.5f), V3(26.0f, -0.02f, 19.5f), mPaving);      // cross path

    const float tr[12][3] = { {-9,12,0}, {-14,13,40}, {-12,26,80}, {-6,27,120}, {9,13,10}, {13,26,50},
                              {10,30,90}, {-18,12,30}, {-9,-7,0}, {9,-7,60}, {14,-4,20}, {-16,-5,100} };
    for (auto& t : tr) tree(t[0], t[1], t[2]);
    const float bushX[8] = { -15, -12, -9, -6, -3, 0, 7, 10 };
    for (float x : bushX) boxC(V3(x, 0.4f, 9.6f), V3(1.3f, 0.8f, 1.0f), mFoliage, V3(0, rnd(m_seed) * 40.0f, 0));

    bench(1.0f, 13.0f, -90.0f);
    bench(6.2f, 24.0f, 90.0f);
    bench(1.0f, 24.5f, -90.0f);

    V3 warm = V3(1.0f, 0.70f, 0.35f) * 0.9f;
    streetLight(6.0f, 12.0f, 180.0f, warm, 1);
    streetLight(1.2f, 21.0f, 0.0f, warm, 0);
    streetLight(6.0f, 30.0f, 180.0f, warm, 2);

    // campus signboard and flag
    box(V3(7.0f, 0, 10.0f), V3(7.1f, 2.0f, 10.1f), mMetalDark);
    box(V3(8.9f, 0, 10.0f), V3(9.0f, 2.0f, 10.1f), mMetalDark);
    signZ(V3(8.0f, 1.6f, 10.05f), 0.5f, "CAMPUS", 4, 0.0f, 0.06f);
    box(V3(-6.05f, 0, 9.95f), V3(-5.95f, 7.0f, 10.05f), mMetalDark);
    boxC(V3(-5.2f, 6.0f, 10.0f), V3(1.5f, 0.9f, 0.02f), mFlag);

    // abandoned things
    car(-5.0f, 22.0f, 25.0f, V3(0.2f, 0.3f, 0.6f));
    boxC(V3(1.3f, 0.22f, 13.4f), V3(0.35f, 0.45f, 0.2f), worldMat(T("metal"), 1.0f, V3(0.7f, 0.15f, 0.1f)), V3(0, 20, 0));
    boxC(V3(3.9f, 0.25f, 22.0f), V3(0.5f, 0.5f, 0.25f), worldMat(T("metal"), 1.0f, V3(0.2f, 0.3f, 0.5f)), V3(0, 40, 0));
    boxC(V3(-2.0f, 0.2f, 15.0f), V3(0.4f, 0.6f, 0.4f), mMetalDark, V3(0, 30, 90));
    barrier(0.5f, 28.0f, 0.0f);
    barrier(6.5f, 28.5f, -10.0f);
    boxC(V3(-1.5f, 0.4f, 32.0f), V3(2.0f, 0.8f, 0.5f), mConcrete, V3(0, 10, 0));
    boxC(V3(7.5f, 0.4f, 32.0f), V3(2.0f, 0.8f, 0.5f), mConcrete, V3(0, -8, 0));
    boxC(V3(9.5f, 1.1f, 30.5f), V3(0.06f, 2.2f, 0.06f), mMetalDark, V3(0, 0, 8));
    boxC(V3(9.4f, 2.0f, 30.46f), V3(0.74f, 0.3f, 0.04f), uvMat(signTex("STOP", 2), 1.0f, 1.0f, V3(1.0f), 8.0f, 0.0f), V3(0, 0, 10));
}

// ---------------------------------------------------------------- Area 4: gate, walls, security booth
void Builder::buildGate()
{
    zone(Zone::Outdoor);
    box(V3(-26.4f, 0, 34.0f), V3(0.8f, 2.6f, 34.4f), mBrick);
    box(V3(6.4f, 0, 34.0f), V3(26.4f, 2.6f, 34.4f), mBrick);
    box(V3(-26.5f, 2.6f, 33.95f), V3(0.8f, 2.72f, 34.45f), mConcrete);
    box(V3(6.4f, 2.6f, 33.95f), V3(26.5f, 2.72f, 34.45f), mConcrete);
    box(V3(-26.4f, 0, -30.4f), V3(-26.0f, 2.6f, 34.4f), mBrick);       // west boundary wall
    box(V3(26.0f, 0, -30.4f), V3(26.4f, 2.6f, 34.4f), mBrick);         // east boundary wall
    box(V3(-26.4f, 0, -30.4f), V3(26.4f, 2.6f, -30.0f), mBrick);       // north boundary wall

    // pillars, sign beam, gate leaves
    box(V3(0.8f, 0, 33.9f), V3(1.6f, 4.0f, 34.5f), mBrick);
    box(V3(5.6f, 0, 33.9f), V3(6.4f, 4.0f, 34.5f), mBrick);
    box(V3(0.7f, 4.0f, 33.8f), V3(1.7f, 4.12f, 34.6f), mConcrete);
    box(V3(5.5f, 4.0f, 33.8f), V3(6.5f, 4.12f, 34.6f), mConcrete);
    // a cold floodlight on the right pillar: lights the gate, the panel side of the booth and the walkway
    int flood = addLight(V3(5.9f, 3.6f, 33.0f), V3(0.85f, 0.92f, 1.0f) * 0.85f, 12.0f, 0);
    box(V3(5.7f, 3.55f, 33.62f), V3(6.3f, 3.85f, 33.9f), fixture(flood, V3(0.9f, 0.95f, 1.0f), 2.0f));
    box(V3(0.8f, 4.12f, 34.0f), V3(6.4f, 6.1f, 34.4f), mConcrete);
    for (int side = 0; side < 2; ++side) {
        float z = side == 0 ? 33.98f : 34.42f;
        signZ(V3(3.6f, 5.55f, z), 0.8f, "BARIDHARA", 4);
        signZ(V3(3.6f, 4.65f, z), 0.7f, "UNIVERSITY", 4);
    }
    box(V3(1.6f, -0.1f, 34.0f), V3(5.6f, 0.1f, 34.4f), mConcrete);                        // threshold
    // Gate leaves: they slide sideways when the gate is unlocked, so they use the mesh's own UVs (the bars
    // travel with the leaf) and carry tags: 2 = left leaf, 3 = right leaf, 7 = the "CLOSED" sign.
    Mat bars = uvMat(T("bars"), 2.0f, 2.3f, V3(0.35f, 0.45f, 0.40f), 48.0f, 0.4f); bars.mode = 1;
    Mat rail = uvMat(T("metal"), 1.0f, 1.0f, V3(0.28f), 48.0f, 0.4f);
    Mat lock = worldMat(T("metal"), 1.0f, V3(0.9f, 0.75f, 0.2f), 64.0f, 0.5f);
    m_tag = 2;
    box(V3(1.6f, 0.1f, 33.80f), V3(3.6f, 2.4f, 33.86f), bars);
    box(V3(1.6f, 2.4f, 33.78f), V3(3.6f, 2.46f, 33.88f), rail);
    m_tag = 3;
    box(V3(3.6f, 0.1f, 33.80f), V3(5.6f, 2.4f, 33.86f), bars);
    box(V3(3.6f, 2.4f, 33.78f), V3(5.6f, 2.46f, 33.88f), rail);
    box(V3(3.45f, 1.0f, 33.75f), V3(3.75f, 1.25f, 33.90f), lock);
    m_tag = 7;
    boxC(V3(2.9f, 1.5f, 33.75f), V3(0.9f, 0.26f, 0.05f), decal(uvMat(signTex("CLOSED", 3), 1.0f, 1.0f, V3(1.0f), 8.0f, 0.0f)), V3(0, 0, 12));
    m_tag = 0;

    // security booth (x -3..-0.6, z 30.6..33)
    box(V3(-3.0f, 0, 30.6f), V3(-0.6f, 0.1f, 33.0f), mConcrete);
    wallSkins(false, 30.6f, 33.0f, -0.65f, 0.1f, 2.7f, { {31.0f, 32.6f, 1.0f, 2.3f} }, Zone::Room, mWallRoom, Zone::Outdoor, mWallOut);
    wallSkins(true, -2.8f, -0.75f, 30.7f, 0.1f, 2.7f, { {-2.4f, -1.6f, 0.1f, 2.2f} }, Zone::Outdoor, mWallOut, Zone::Room, mWallRoom);
    wallSkins(true, -2.8f, -0.75f, 32.9f, 0.1f, 2.7f, {}, Zone::Room, mWallRoom, Zone::Outdoor, mWallOut);
    wallSkins(false, 30.6f, 33.0f, -2.9f, 0.1f, 2.7f, {}, Zone::Outdoor, mWallOut, Zone::Room, mWallRoom);
    window(false, 31.0f, 32.6f, -0.65f, 1.0f, 2.3f);
    zone(Zone::Outdoor);
    box(V3(-3.2f, 2.7f, 30.4f), V3(-0.4f, 2.85f, 33.2f), mConcrete);
    signX(V3(-0.535f, 2.45f, 31.8f), 0.28f, "SECURITY", 4);
    zone(Zone::Room);
    box(V3(-1.3f, 0.8f, 31.4f), V3(-0.85f, 0.85f, 32.6f), mDeskTop);
    box(V3(-1.3f, 0.1f, 31.4f), V3(-1.25f, 0.8f, 32.6f), mWoodDark);
    box(V3(-2.2f, 0.4f, 31.5f), V3(-1.75f, 0.45f, 31.95f), mWoodDark);
    ammoCrate(V3(-2.45f, 0.1f, 32.45f), 0.0f, 3);                // ammunition: inside the security booth
    // access terminal on the booth's west wall (red = locked, green = unlocked; see Level::setPanelUnlocked)
    box(V3(-2.8f, 1.0f, 31.45f), V3(-2.72f, 1.5f, 32.15f), mMetalDark);
    Mat redLed = worldMat(T("concrete"), 1.0f, V3(1.0f, 0.10f, 0.05f), 8.0f, 0.0f);   redLed.emissive = 1.8f; redLed = decal(redLed);
    Mat greenLed = worldMat(T("concrete"), 1.0f, V3(0.10f, 1.0f, 0.25f), 8.0f, 0.0f); greenLed.emissive = 1.8f; greenLed = decal(greenLed);
    m_tag = 4; boxC(V3(-2.71f, 1.35f, 31.80f), V3(0.02f, 0.14f, 0.14f), redLed);
    m_tag = 5; boxC(V3(-2.71f, 1.35f, 31.80f), V3(0.02f, 0.14f, 0.14f), greenLed);
    m_tag = 0;
    int li = addLight(V3(-1.8f, 2.4f, 31.8f), V3(1.0f, 0.8f, 0.5f) * 0.8f, 5.0f, 1);
    box(V3(-2.1f, 2.62f, 31.5f), V3(-1.5f, 2.7f, 32.1f), fixture(li, V3(1.0f, 0.85f, 0.6f)));
}

// ---------------------------------------------------------------- road, pavements, city backdrop
void Builder::buildRoad()
{
    zone(Zone::Outdoor);
    box(V3(-120, -0.5f, 36.0f), V3(120, -0.08f, 44.5f), mAsphalt);
    box(V3(-120, -0.5f, 34.4f), V3(120, 0.1f, 36.0f), mPaving);
    box(V3(-120, -0.5f, 44.5f), V3(120, 0.1f, 47.0f), mPaving);
    Mat white = mConcrete; white.tint = V3(1.3f, 1.3f, 1.2f);
    for (int i = -10; i < 10; ++i) box(V3(i * 6.0f, -0.08f, 40.15f), V3(i * 6.0f + 3.0f, -0.065f, 40.25f), white);   // centre dashes
    for (int i = 0; i < 6; ++i) box(V3(9.5f + i * 0.8f, -0.08f, 36.4f), V3(9.95f + i * 0.8f, -0.065f, 43.6f), white); // zebra crossing

    // the safe checkpoint (hidden until the gate has been unlocked): two glowing posts, a sign and a floor marker
    zone(Zone::Outdoor);
    Mat glow = worldMat(T("concrete"), 1.0f, V3(0.2f, 1.0f, 0.4f), 8.0f, 0.0f); glow.emissive = 1.5f; glow = decal(glow);
    const V3 cp = sites::kCheckpoint;
    m_tag = 6;
    box(V3(cp.x - 2.35f, 0.0f, cp.z - 0.15f), V3(cp.x - 2.05f, 4.0f, cp.z + 0.15f), glow);
    box(V3(cp.x + 2.05f, 0.0f, cp.z - 0.15f), V3(cp.x + 2.35f, 4.0f, cp.z + 0.15f), glow);
    signZ(V3(cp.x, 4.45f, cp.z), 0.55f, "SAFE CHECKPOINT", 0, 1.3f, 0.10f);
    box(V3(cp.x - 1.8f, -0.075f, cp.z - 1.8f), V3(cp.x + 1.8f, -0.06f, cp.z - 1.65f), glow);
    box(V3(cp.x - 1.8f, -0.075f, cp.z + 1.65f), V3(cp.x + 1.8f, -0.06f, cp.z + 1.8f), glow);
    box(V3(cp.x - 1.8f, -0.075f, cp.z - 1.65f), V3(cp.x - 1.65f, -0.06f, cp.z + 1.65f), glow);
    box(V3(cp.x + 1.65f, -0.075f, cp.z - 1.65f), V3(cp.x + 1.8f, -0.06f, cp.z + 1.65f), glow);
    m_checkpointLight = addLight(V3(cp.x, 2.5f, cp.z), V3(0.3f, 1.0f, 0.45f) * 0.9f, 11.0f, 1);
    m_tag = 0;

    V3 warm = V3(1.0f, 0.70f, 0.35f) * 0.9f;
    streetLight(-16.0f, 35.2f, -90.0f, warm, 1);
    streetLight(14.0f, 35.2f, -90.0f, warm, 2);
    streetLight(30.0f, 35.2f, -90.0f, warm, 0);

    bus(-14.0f, 38.1f, 0.0f);
    cng(8.0f, 42.0f, 170.0f);
    car(24.0f, 38.0f, 8.0f, V3(0.7f, 0.7f, 0.72f));
    car(-30.0f, 42.0f, 185.0f, V3(0.2f, 0.2f, 0.25f));
    rickshaw(-2.0f, 36.8f, 70.0f);
    rickshaw(18.0f, 43.0f, -100.0f);
    barrier(0.2f, 35.2f, 0.0f);
    barrier(7.0f, 35.0f, -8.0f);

    apartment(-60, -44, 50, 62, 16, V3(1.0f, 0.85f, 0.85f));
    apartment(-44, -30, 50, 62, 12, V3(0.85f, 0.95f, 1.0f));
    apartment(-30, -14, 50, 62, 20, V3(1.0f, 0.95f, 0.8f));
    apartment(-14, 2, 50, 62, 14, V3(0.9f, 1.0f, 0.9f));
    apartment(2, 18, 50, 62, 18, V3(1.0f, 0.88f, 0.75f));
    apartment(18, 34, 50, 62, 11, V3(0.85f, 0.85f, 1.0f));
    apartment(34, 50, 50, 62, 15, V3(1.0f, 0.9f, 0.9f));
    apartment(50, 66, 50, 62, 19, V3(0.95f, 0.95f, 0.85f));
}

void Builder::build()
{
    loadTextures();
    makeMats();
    buildGround();
    buildClassroom();
    buildCorridor();
    buildStairs();
    buildFacade();
    buildCourtyard();
    buildGate();
    buildRoad();
}

} // anonymous namespace

// ====================================================================== Level
// The model matrix of a prop: move (position + offset), rotate (Y, X, Z), then scale.
static glm::mat4 propModel(const Prop& p)
{
    glm::mat4 model = glm::translate(glm::mat4(1.0f), p.pos + p.offset);
    if (p.rot.y != 0.0f) model = glm::rotate(model, glm::radians(p.rot.y), glm::vec3(0, 1, 0));
    if (p.rot.x != 0.0f) model = glm::rotate(model, glm::radians(p.rot.x), glm::vec3(1, 0, 0));
    if (p.rot.z != 0.0f) model = glm::rotate(model, glm::radians(p.rot.z), glm::vec3(0, 0, 1));
    return glm::scale(model, p.size);
}

Level::Level() : m_cube(Mesh::createCube(1.0f))
{
    Builder builder(m_tex, m_props, m_lights, m_cardLight, m_checkpointLight);
    builder.build();

    // Collision: every solid prop becomes one AABB. For rotated props we transform the 8 corners of the
    // unit cube by the model matrix and take the smallest axis-aligned box around them.
    for (size_t idx = 0; idx < m_props.size(); ++idx) {
        const Prop& p = m_props[idx];
        if (p.tag > 0 && p.tag < 16) m_tagProps[p.tag].push_back((int)idx);
        if (!p.solid) continue;
        glm::mat4 model = propModel(p);
        glm::vec3 mn(1.0e9f), mx(-1.0e9f);
        for (int i = 0; i < 8; ++i) {
            glm::vec3 corner((i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f);
            glm::vec3 w = glm::vec3(model * glm::vec4(corner, 1.0f));
            mn = glm::min(mn, w);
            mx = glm::max(mx, w);
        }
        AABB box; box.min = mn; box.max = mx;
        int id = m_collision.add(box);
        if (p.tag == 2 || p.tag == 3) m_gateCollision.push_back(id);
    }
    // one spare light that follows the pistol's muzzle flash
    PointLight flash;
    flash.color = glm::vec3(1.0f, 0.8f, 0.45f);
    flash.radius = 9.0f;
    flash.enabled = false;
    m_flashLight = (int)m_lights.size();
    m_lights.push_back(flash);

    resetMission();
}

void Level::setMuzzleLight(const glm::vec3& position, float strength)
{
    if (m_flashLight < 0) return;
    PointLight& l = m_lights[m_flashLight];
    l.pos = position;
    l.color = glm::vec3(1.0f, 0.8f, 0.45f) * (strength * 1.6f);
    l.enabled = strength > 0.01f;
}

// ---------------------------------------------------------------- mission props
void Level::setTagVisible(int tag, bool visible)
{
    for (int i : m_tagProps[tag]) m_props[i].visible = visible;
}

void Level::setCardVisible(bool visible)
{
    setTagVisible(1, visible);
    if (m_cardLight >= 0) m_lights[m_cardLight].enabled = visible;
}

void Level::setPanelUnlocked(bool unlocked)
{
    setTagVisible(4, !unlocked);
    setTagVisible(5, unlocked);
}

void Level::setAmmoVisible(int index, bool visible)
{
    if (index >= 0 && index < 4) setTagVisible(8 + index, visible);
}

void Level::setCheckpointActive(bool active)
{
    setTagVisible(6, active);
    if (m_checkpointLight >= 0) m_lights[m_checkpointLight].enabled = active;
}

void Level::setGateProgress(float t)
{
    t = std::max(0.0f, std::min(1.0f, t));
    if (t == m_gateProgress) return;
    m_gateProgress = t;
    float eased = t * t * (3.0f - 2.0f * t);
    for (int i : m_tagProps[2]) m_props[i].offset = glm::vec3(-2.0f * eased, 0.0f, 0.0f);   // left leaf slides left
    for (int i : m_tagProps[3]) m_props[i].offset = glm::vec3( 2.0f * eased, 0.0f, 0.0f);   // right leaf slides right
    setTagVisible(7, t <= 0.0f);                                                             // "CLOSED" sign disappears
    bool blocking = t < 0.6f;                                                                // passable once mostly open
    for (int id : m_gateCollision) m_collision.setEnabled(id, blocking);
}

void Level::resetMission()
{
    m_gateProgress = -1.0f;      // force setGateProgress to apply
    setCardVisible(true);
    setPanelUnlocked(false);
    setGateProgress(0.0f);
    setCheckpointActive(false);
    for (int i = 0; i < 4; ++i) setAmmoVisible(i, true);
}

// ---------------------------------------------------------------- lighting for characters
void Level::lightingAt(const glm::vec3& p, float& sun, float& amb) const
{
    sun = 1.0f; amb = 1.0f;                                                                       // outdoors
    if (p.x > -5.2f && p.x < 5.2f && p.z > -4.2f && p.z < 4.0f && p.y < 3.3f) { sun = 0.12f; amb = 1.0f; }       // classroom
    else if (p.x > -19.2f && p.x < 12.2f && p.z >= 4.0f && p.z < 7.3f && p.y < 6.8f) { sun = 0.04f; amb = 0.55f; } // corridor + stairs
    else if (p.x > -3.0f && p.x < -0.6f && p.z > 30.6f && p.z < 33.0f) { sun = 0.12f; amb = 1.0f; }             // security booth
}

// ---------------------------------------------------------------- drawing
void Level::drawProp(const Shader& shader, const Prop& p) const
{
    if (p.tex != m_boundTex) { p.tex->bind(0); m_boundTex = p.tex; }

    float glow = p.emissive;
    if (p.light >= 0) glow *= m_lights[p.light].intensity;   // lamps dim when their light flickers

    shader.setMat4("uModel", propModel(p));
    shader.setVec2("uTexScale", p.uv);
    shader.setVec3("uTint", p.tint);
    shader.setFloat("uShininess", p.shininess);
    shader.setFloat("uSpecular", p.specular);
    shader.setFloat("uSunScale", p.sunScale);
    shader.setFloat("uAmbientScale", p.ambientScale);
    shader.setFloat("uEmissive", glow);
    shader.setFloat("uAlpha", p.alpha);
    shader.setInt("uUvMode", p.uvMode);
    shader.setInt("uMode", p.mode);
    m_cube.draw();
}

void Level::drawOpaque(const Shader& shader, const glm::vec3& cameraPos, float time)
{
    // the access card spins and bobs so it catches the eye
    for (int i : m_tagProps[1]) {
        m_props[i].rot.y = time * 90.0f;
        m_props[i].offset.y = std::sin(time * 2.0f) * 0.06f;
    }

    // 1. update flicker, then send the 8 enabled lights nearest to the camera
    for (PointLight& l : m_lights) l.intensity = flickerValue(l.flicker, l.phase, time);

    std::vector<int> order;
    for (size_t i = 0; i < m_lights.size(); ++i) if (m_lights[i].enabled) order.push_back((int)i);
    auto d2 = [&](int i) { glm::vec3 d = m_lights[i].pos - cameraPos; return glm::dot(d, d); };
    std::sort(order.begin(), order.end(), [&](int a, int b) { return d2(a) < d2(b); });
    int count = (int)std::min<size_t>(8, order.size());
    for (int k = 0; k < count; ++k) {
        const PointLight& l = m_lights[order[k]];
        std::string idx = "[" + std::to_string(k) + "]";
        shader.setVec3("uLightPos" + idx, l.pos);
        shader.setVec3("uLightColor" + idx, l.color * l.intensity);
        shader.setFloat("uLightRadius" + idx, l.radius);
    }
    shader.setInt("uLightCount", count);

    // 2. opaque + cut-out props
    m_boundTex = nullptr;
    for (const Prop& p : m_props)
        if (p.visible && p.mode != 2) drawProp(shader, p);
}

void Level::drawGlass(const Shader& shader, const glm::vec3& cameraPos)
{
    m_boundTex = nullptr;    // someone else (the player) may have bound a different texture meanwhile

    // glass: sorted far -> near, blended, without writing depth
    std::vector<std::pair<float, const Prop*>> glass;
    for (const Prop& p : m_props)
        if (p.visible && p.mode == 2) {
            glm::vec3 d = p.pos - cameraPos;
            glass.push_back({ glm::dot(d, d), &p });
        }
    std::sort(glass.begin(), glass.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (auto& g : glass) drawProp(shader, *g.second);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
