// src/world/ProceduralTextures.cpp
#include "world/ProceduralTextures.h"
#include "ui/Font.h"

#include <cmath>
#include <functional>
#include <vector>

using glm::vec3;
using glm::vec4;

// deterministic pseudo-random number (0..1) from two integers
static float hash2(int x, int y)
{
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return (h & 0xFFFFu) / 65535.0f;
}

static Texture makeRGBA(int w, int h, const std::function<vec4(int, int)>& fn)
{
    std::vector<unsigned char> data((size_t)w * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            vec4 c = glm::clamp(fn(x, y), 0.0f, 1.0f);
            size_t i = ((size_t)y * w + x) * 4;
            data[i + 0] = (unsigned char)(c.r * 255.0f);
            data[i + 1] = (unsigned char)(c.g * 255.0f);
            data[i + 2] = (unsigned char)(c.b * 255.0f);
            data[i + 3] = (unsigned char)(c.a * 255.0f);
        }
    return Texture(w, h, 4, data.data());
}

namespace ProcTex {

Texture plaster()
{
    return Texture::fromFunction(128, 128, [](int x, int y) {
        float g = 0.82f + 0.08f * hash2(x / 8, y / 8) + 0.05f * hash2(x, y);
        vec3 c(g, g * 0.97f, g * 0.88f);
        // dirty streaks running down from the top of the tile (y grows upward)
        if (hash2(x / 3, 77) > 0.7f) c *= 1.0f - 0.30f * (float)y / 127.0f;
        // grime near the floor
        if (y < 20) c *= 0.72f + 0.28f * (float)y / 20.0f;
        // chips / damage specks
        if (hash2(x * 7 + 3, y * 5 + 1) > 0.992f) c *= 0.5f;
        return c;
    });
}

Texture floorTile()
{
    return Texture::fromFunction(128, 128, [](int x, int y) {
        int tx = x / 32, ty = y / 32;
        bool light = ((tx + ty) % 2) == 0;
        vec3 base = light ? vec3(0.72f, 0.70f, 0.64f) : vec3(0.42f, 0.45f, 0.44f);
        float tone = 0.9f + 0.2f * hash2(tx * 13 + 1, ty * 7 + 2);
        float grain = 0.93f + 0.07f * hash2(x, y);
        vec3 c = base * tone * grain;
        if (hash2(x / 2, y / 2) > 0.96f) c *= 0.8f;                  // scuffs
        if (x % 32 == 0 || y % 32 == 0) c = vec3(0.22f, 0.22f, 0.21f);   // grout
        return c;
    });
}

Texture asphalt()
{
    return Texture::fromFunction(128, 128, [](int x, int y) {
        float g = 0.17f + 0.05f * hash2(x / 2, y / 2) + 0.05f * hash2(x, y);
        if (hash2(x * 3, y * 3 + 9) > 0.985f) g += 0.25f;   // bright stones in the asphalt
        return vec3(g, g, g * 1.03f);
    });
}

Texture paving()
{
    return Texture::fromFunction(128, 128, [](int x, int y) {
        int row = y / 8;
        int xo = (x + (row % 2) * 8) % 128;
        if ((xo % 16) == 0 || (y % 8) == 0) return vec3(0.25f, 0.24f, 0.23f);
        float t = 0.8f + 0.3f * hash2(xo / 16, row);
        return vec3(0.58f, 0.50f, 0.45f) * t * (0.92f + 0.08f * hash2(x, y));
    });
}

Texture metal()
{
    return Texture::fromFunction(128, 128, [](int x, int y) {
        float brushed = 0.9f + 0.1f * hash2(x / 2, y);
        vec3 c = vec3(0.55f, 0.58f, 0.60f) * brushed;
        if (hash2(x / 10, y / 10) > 0.78f)                      // rust patches
            c = vec3(0.45f, 0.25f, 0.10f) * (0.7f + 0.5f * hash2(x, y));
        return c;
    });
}

Texture foliage()
{
    return Texture::fromFunction(128, 128, [](int x, int y) {
        float r = hash2(x / 3, y / 3);
        vec3 c = (r < 0.33f) ? vec3(0.10f, 0.30f, 0.08f)
               : (r < 0.66f) ? vec3(0.18f, 0.42f, 0.10f)
                             : vec3(0.26f, 0.50f, 0.15f);
        return c * (0.85f + 0.15f * hash2(x, y));
    });
}

Texture door()
{
    return Texture::fromFunction(128, 128, [](int x, int y) {
        vec3 c = vec3(0.42f, 0.26f, 0.13f) * (0.9f + 0.1f * std::sin(x * 0.7f + hash2(x / 4, 5) * 6.0f));
        bool inPanel = (x >= 16 && x < 112) && ((y >= 12 && y < 58) || (y >= 70 && y < 116));
        if (inPanel) {
            bool edge = (x < 19 || x >= 109 || (y >= 12 && y < 15) || (y >= 55 && y < 58) ||
                         (y >= 70 && y < 73) || (y >= 113 && y < 116));
            c *= edge ? 0.6f : 0.85f;
        }
        return c;
    });
}

static Texture windowImpl(bool seeThrough)
{
    return makeRGBA(128, 128, [seeThrough](int x, int y) {
        bool frame = (x < 8 || x > 119 || y < 8 || y > 119 ||
                      (x >= 62 && x <= 65) || (y >= 62 && y <= 65));
        if (frame) return vec4(0.30f, 0.30f, 0.33f, 1.0f);
        bool glare = ((x + y) % 50) < 8;
        if (seeThrough) {
            float a = glare ? 0.38f : 0.20f;
            return vec4(0.65f, 0.78f, 0.85f, a);
        }
        float k = glare ? 0.30f : 0.12f;
        return vec4(k * 0.8f, k, k * 1.3f, 1.0f);
    });
}
Texture windowGlass() { return windowImpl(true); }
Texture windowDark()  { return windowImpl(false); }

Texture facade()
{
    return Texture::fromFunction(128, 128, [](int x, int y) {
        float g = 0.80f + 0.08f * hash2(x / 6, y / 6) + 0.05f * hash2(x, y);
        vec3 c(g, g * 0.96f, g * 0.9f);
        c *= 1.0f - 0.25f * (hash2(x / 4, 33) > 0.75f ? (float)y / 127.0f : 0.0f);   // soot streaks
        if (y >= 24 && y < 28) c = vec3(0.55f);                                      // ledge
        for (int w = 0; w < 2; ++w) {
            int x0 = 14 + w * 64;
            if (x >= x0 && x < x0 + 36 && y >= 36 && y < 96) {
                bool frame = (x < x0 + 3 || x >= x0 + 33 || y < 39 || y >= 93 || (y >= 64 && y < 67));
                c = frame ? vec3(0.90f) : vec3(0.10f, 0.13f, 0.17f);
            }
        }
        if (x >= 84 && x < 104 && y >= 40 && y < 54) c = vec3(0.70f, 0.70f, 0.68f);  // AC unit
        return c;
    });
}

Texture notice()
{
    return Texture::fromFunction(128, 128, [](int x, int y) {
        vec3 paper(0.90f, 0.90f, 0.85f);
        if (y >= 104 && y < 116) return vec3(0.65f, 0.12f, 0.10f);                    // red header band
        int row = (y - 14) / 8;
        if (y >= 14 && y < 98 && ((y - 14) % 8) < 3) {
            int len = 30 + (int)(hash2(row, 1) * 76.0f);
            if (x >= 10 && x < 10 + len) return vec3(0.25f);
        }
        return paper * (0.95f + 0.05f * hash2(x, y));
    });
}

Texture hazard()
{
    return Texture::fromFunction(128, 128, [](int x, int y) {
        return (((x + y) / 16) % 2 == 0) ? vec3(0.80f, 0.12f, 0.10f) : vec3(0.88f);
    });
}

Texture gateBars()
{
    return makeRGBA(128, 128, [](int x, int y) {
        bool bar  = (x % 16) < 8;
        bool rail = (y < 6);
        if (bar || rail) return vec4(0.25f, 0.27f, 0.28f, 1.0f);
        return vec4(0.0f);
    });
}

Texture flag()
{
    const int W = 160, H = 96;
    return Texture::fromFunction(W, H, [](int x, int y) {
        float dx = (float)x - 72.0f, dy = (float)y - 48.0f;
        if (dx * dx + dy * dy < 20.0f * 20.0f) return vec3(0.96f, 0.17f, 0.21f);
        return vec3(0.0f, 0.42f, 0.31f);
    });
}

// ---------------- pixel font ----------------
namespace {
const unsigned char* findGlyph(char c) { return Font::glyphRows(c); }
const int kPad = 2;    // blank cells around the text
const int kScale = 4;  // texture pixels per font cell
}

float signAspect(const std::string& text)
{
    int n = (int)text.size();
    float gw = (float)(n * 6 - 1 + 2 * kPad);
    float gh = (float)(7 + 2 * kPad);
    return gw / gh;
}

Texture sign(const std::string& text, glm::vec3 fg, glm::vec3 bg)
{
    int n = (int)text.size();
    int gridW = n * 6 - 1 + 2 * kPad;
    int gridH = 7 + 2 * kPad;
    int w = gridW * kScale, h = gridH * kScale;

    return Texture::fromFunction(w, h, [=](int x, int y) {
        int gx = x / kScale;
        int gy = (h - 1 - y) / kScale;       // texture row 0 is the BOTTOM of the sign
        if (gx == 0 || gy == 0 || gx == gridW - 1 || gy == gridH - 1) return fg;   // border
        int tx = gx - kPad, ty = gy - kPad;
        if (tx >= 0 && ty >= 0 && ty < 7) {
            int ci = tx / 6, col = tx % 6;
            if (ci < n && col < 5) {
                const unsigned char* g = findGlyph(text[ci]);
                if (g && ((g[ty] >> (4 - col)) & 1)) return fg;
            }
        }
        return bg * (0.95f + 0.05f * hash2(x, y));
    });
}

} // namespace ProcTex
