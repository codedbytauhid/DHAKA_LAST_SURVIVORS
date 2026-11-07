// src/game/ZombieManager.cpp
#include "game/ZombieManager.h"
#include "glad.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>

using glm::vec3;

namespace {

struct Spawn { float x, y, z, radius; bool runner; const char* where; };

// Where the zombies start. y = height of the floor there (0 ground floor, 1.6 landing, 3.4 upper hall).
// Order matters: spawn(8) uses the first eight, which are spread over the whole map.
const Spawn kSpawns[12] = {
    { -4.3f, 0.0f,  3.2f, 1.8f, false, "classroom, back corner" },
    { 10.8f, 0.0f,  5.6f, 2.5f, false, "corridor, near the east exit" },
    { -5.5f, 0.0f, 14.5f, 3.0f, false, "courtyard, near the flag" },
    {  5.2f, 0.0f, 32.6f, 2.5f, true,  "by the gate" },
    {-10.5f, 0.0f,  5.4f, 1.5f, false, "corridor, by the lockers" },
    {  8.5f, 0.0f, 20.5f, 3.0f, false, "courtyard, east lawn" },
    {-17.5f, 1.6f,  6.5f, 1.2f, false, "staircase landing" },
    {  2.2f, 0.0f, -2.7f, 1.5f, false, "classroom, front" },
    {  3.6f, 0.0f, 16.5f, 3.0f, true,  "courtyard walkway" },
    { -3.0f, 0.0f, 26.0f, 3.0f, false, "courtyard, near the abandoned car" },
    { -1.0f, 0.0f,  6.0f, 3.0f, false, "corridor, middle" },
    { -8.2f, 3.4f,  6.3f, 1.0f, false, "upper hall, near the barricade" },
};

// look variations (indexed by zombie id)
const vec3 kShirts[6] = { {0.20f,0.14f,0.11f}, {0.16f,0.19f,0.26f}, {0.32f,0.12f,0.10f},
                          {0.20f,0.27f,0.18f}, {0.33f,0.32f,0.28f}, {0.12f,0.12f,0.14f} };
const vec3 kPants[4]  = { {0.10f,0.10f,0.14f}, {0.20f,0.16f,0.10f}, {0.12f,0.14f,0.12f}, {0.25f,0.25f,0.28f} };
const vec3 kSkins[3]  = { {0.52f,0.66f,0.50f}, {0.62f,0.64f,0.60f}, {0.60f,0.65f,0.42f} };   // green / grey / sickly yellow

float lerpf(float a, float b, float t) { return a + (b - a) * t; }

} // namespace

ZombieManager::ZombieManager()
    : m_cube(Mesh::createCube(1.0f)),
      m_cloth(Texture::fromFunction(32, 32, [](int x, int y) {          // rough, dirty cloth noise
          unsigned int h = ((unsigned int)x * 2654435761u) ^ ((unsigned int)y * 40503u * 2246822519u);
          float g = 0.70f + 0.30f * ((h >> 5) & 255u) / 255.0f;
          return glm::vec3(g);
      })),
      m_nav(NavGraph::createUniversity())
{
}

void ZombieManager::spawn(int count, const CollisionWorld& world)
{
    m_zombies.clear();
    count = std::max(0, std::min(count, 12));
    for (int i = 0; i < count; ++i) {
        const Spawn& s = kSpawns[i];
        // sanity check against the real level: is there floor, and is the spot free of walls/furniture?
        float g = world.groundHeightAt(s.x, s.z, s.y + 0.4f, 0.3f);
        bool clear = world.isClear(s.x, g, s.z, 0.30f, 1.75f, 0.35f);
        if (g < -1.0e8f || !clear)
            std::fprintf(stderr, "[Zombies] WARNING: spawn %d (%s) at %.1f %.1f is %s\n", i, s.where, s.x, s.z,
                         g < -1.0e8f ? "over nothing" : "inside something");
        m_zombies.emplace_back(i, vec3(s.x, g > -1.0e8f ? g : s.y, s.z), s.radius, s.runner);
        m_zombies.back().setMaxHealth(s.runner ? config.runnerHealth : config.health);
        m_zombies.back().reset();
    }
    std::printf("[Zombies] spawned %d zombies\n", (int)m_zombies.size());
}

void ZombieManager::reset()
{
    for (Zombie& z : m_zombies) z.reset();
}

int ZombieManager::alive() const
{
    int n = 0;
    for (const Zombie& z : m_zombies) if (z.alive()) ++n;
    return n;
}

int ZombieManager::countState(ZombieState s) const
{
    int n = 0;
    for (const Zombie& z : m_zombies) if (z.state() == s) ++n;
    return n;
}

void ZombieManager::alertNoise(const vec3& at, float radius)
{
    for (Zombie& z : m_zombies) {
        if (!z.alive()) continue;
        float dx = z.position().x - at.x, dz = z.position().z - at.z;
        if (dx * dx + dz * dz <= radius * radius && std::fabs(z.position().y - at.y) < 3.5f) z.hearNoise();
    }
}

bool ZombieManager::killNearest(const vec3& from, float maxDist)
{
    int best = -1;
    float bestD = maxDist;
    for (size_t i = 0; i < m_zombies.size(); ++i) {
        if (!m_zombies[i].alive()) continue;
        const vec3& p = m_zombies[i].position();
        float d = std::sqrt((p.x - from.x) * (p.x - from.x) + (p.z - from.z) * (p.z - from.z));
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    if (best < 0) return false;
    m_zombies[best].kill();
    emit(ZombieEvent::Death, m_zombies[best].position());
    return true;
}

float ZombieManager::update(float dt, const vec3& playerPos, float playerSpeed, bool playerAlive,
                            const CollisionWorld& world)
{
    float damage = 0.0f;
    const int n = (int)m_zombies.size();
    for (int i = 0; i < n; ++i) {
        Zombie& z = m_zombies[i];

        // Separation: zombies gently push each other apart so they do not stand inside one another.
        glm::vec2 push(0.0f);
        if (z.alive()) {
            for (int j = 0; j < n; ++j) {
                if (j == i || !m_zombies[j].alive()) continue;
                float dx = z.position().x - m_zombies[j].position().x;
                float dz = z.position().z - m_zombies[j].position().z;
                float d = std::sqrt(dx * dx + dz * dz);
                if (d < 0.9f && d > 0.001f && std::fabs(z.position().y - m_zombies[j].position().y) < 1.0f) {
                    float k = (0.9f - d) / 0.9f * 1.5f;
                    push.x += dx / d * k;
                    push.y += dz / d * k;
                }
            }
        }

        ZombieContext c;
        c.playerPos = playerPos;
        c.playerSpeed = playerSpeed;
        c.playerAlive = playerAlive;
        c.push = push;
        c.world = &world;
        c.nav = &m_nav;
        c.cfg = &config;

        ZombieOutput out = z.update(dt, c);
        damage += out.damage;
        if (out.detected) emit(ZombieEvent::Detect, z.position());
        if (out.attacked) emit(ZombieEvent::Attack, z.position());
        if (out.groaned)  emit(ZombieEvent::Groan, z.position());
    }
    return damage;
}

// ====================================================================== drawing
void ZombieManager::draw(const Shader& shader, const Level& level, const vec3& cameraPos) const
{
    m_cloth.bind(0);
    shader.setVec2("uTexScale", glm::vec2(1.0f));
    shader.setInt("uUvMode", 1);
    shader.setInt("uMode", 0);
    shader.setFloat("uShininess", 8.0f);
    shader.setFloat("uSpecular", 0.02f);
    shader.setFloat("uAlpha", 1.0f);
    shader.setFloat("uEmissive", 0.0f);

    const float kPi = 3.14159265f;

    for (const Zombie& z : m_zombies) {
        const vec3& pos = z.position();
        float cx = pos.x - cameraPos.x, cz = pos.z - cameraPos.z;
        if (cx * cx + cz * cz > 80.0f * 80.0f) continue;          // too far away to matter

        float sun, amb;
        level.lightingAt(pos + vec3(0.0f, 0.9f, 0.0f), sun, amb);
        shader.setFloat("uSunScale", sun);
        shader.setFloat("uAmbientScale", amb);

        // ---- look (varies with the id)
        int id = z.id();
        float dirt = 0.8f + 0.2f * (float)((id * 7) % 5) / 4.0f;
        vec3 shirt = kShirts[id % 6] * dirt;
        vec3 pants = kPants[(id * 3 + 1) % 4] * dirt;
        vec3 skin  = kSkins[(id / 2 + id) % 3];
        vec3 hair(0.06f + 0.05f * (id % 3), 0.05f + 0.04f * (id % 3), 0.05f);
        vec3 shoes(0.08f), blood(0.30f, 0.03f, 0.04f);
        bool hasBlood = (id % 2 == 0), hasHair = (id % 4 != 3);
        float scale = 0.95f + 0.10f * (float)((id * 37) % 10) / 10.0f;

        // ---- pose
        float t = z.time();
        float mb = z.moveBlend(), rb = z.runBlend();
        float sw = std::sin(z.phase());
        float legAmp = (0.22f + 0.50f * rb) * mb;
        float legL = sw * legAmp, legR = -sw * legAmp * z.limp();          // one leg drags a little
        float armBase = -0.45f + (-0.80f - 0.10f * rb) * mb;               // idle: hanging forward, moving: reaching out
        float armSwing = std::sin(z.phase() + 0.8f) * 0.28f * mb * (0.6f + rb);
        float idleSway = std::sin(t * 1.3f) * 0.10f * (1.0f - mb);
        float armL = armBase + armSwing + idleSway, armR = armBase - armSwing - idleSway * 0.7f;
        float lean = 0.38f + 0.12f * rb;
        float bob = std::fabs(sw) * 0.04f * mb;
        float roll = std::sin(z.phase() * 0.5f) * 0.06f * mb + std::sin(t * 0.9f) * 0.02f;   // lurching side to side
        float headDroop = 0.28f + 0.1f * std::sin(t * 0.7f);

        float sp = z.swingProgress();                                       // attack animation: raise, then strike down
        if (sp >= 0.0f) {
            float a = (sp < 0.5f) ? lerpf(-1.3f, -2.5f, sp / 0.5f) : lerpf(-2.5f, -0.5f, (sp - 0.5f) / 0.5f);
            armL = armR = a;
            lean += 0.25f * std::sin(sp * kPi);
        } else if (z.state() == ZombieState::Attack) {
            armL = -1.3f + std::sin(t * 3.0f) * 0.10f;                      // waiting between blows, arms up
            armR = -1.3f - std::sin(t * 3.0f) * 0.10f;
        }

        float hit = z.hitFlash();                                           // just been shot: flinch and flash red
        if (hit > 0.0f && z.state() != ZombieState::Dead) {
            lean -= 0.5f * hit;
            headDroop -= 0.6f * hit;
            roll += std::sin(t * 60.0f) * 0.10f * hit;
            skin += (vec3(0.90f, 0.20f, 0.18f) - skin) * (hit * 0.6f);
            shirt += (vec3(0.60f, 0.12f, 0.10f) - shirt) * (hit * 0.4f);
        }

        float dead = z.deathProgress();
        float fall = 0.0f, lift = 0.0f;
        if (z.state() == ZombieState::Dead) {
            fall = dead * dead * (kPi * 0.5f);                              // topples forward, faster and faster
            lift = 0.14f * dead * dead;                                     // chest rests on the floor
            lean *= (1.0f - dead); roll *= (1.0f - dead); bob = 0.0f; legL = legR = 0.0f;
            armL = lerpf(armL, -0.3f, dead); armR = lerpf(armR, -0.6f, dead);
            headDroop = lerpf(headDroop, 0.0f, dead);
        }

        // ---- frames
        glm::mat4 base = glm::translate(glm::mat4(1.0f), pos + vec3(0.0f, bob + lift, 0.0f));
        base = glm::rotate(base, z.facing(), vec3(0, 1, 0));
        base = glm::rotate(base, fall, vec3(1, 0, 0));
        base = glm::scale(base, vec3(scale));

        glm::mat4 upper = glm::translate(base, vec3(0.0f, 0.85f, 0.0f));
        upper = glm::rotate(upper, roll, vec3(0, 0, 1));
        upper = glm::rotate(upper, lean, vec3(1, 0, 0));

        auto part = [&](const glm::mat4& f, vec3 c, vec3 s, vec3 col) {
            glm::mat4 m = glm::scale(glm::translate(f, c), s);
            shader.setMat4("uModel", m);
            shader.setVec3("uTint", col);
            m_cube.draw();
        };

        // torso, ragged hem, torn chest, stain
        part(upper, vec3(0.0f, 0.30f, 0.0f), vec3(0.42f, 0.60f, 0.22f), shirt);
        part(upper, vec3(0.0f, 0.00f, 0.0f), vec3(0.44f, 0.12f, 0.24f), shirt * 0.55f);
        part(upper, vec3(0.08f, 0.38f, 0.113f), vec3(0.17f, 0.20f, 0.02f), skin * 0.9f);
        if (hasBlood) part(upper, vec3(-0.09f, 0.20f, 0.114f), vec3(0.15f, 0.13f, 0.02f), blood);

        // head: droops forward and turns to look around
        glm::mat4 head = glm::translate(upper, vec3(0.0f, 0.63f, 0.0f));
        head = glm::rotate(head, z.headYaw(), vec3(0, 1, 0));
        head = glm::rotate(head, headDroop, vec3(1, 0, 0));
        part(upper, vec3(0.0f, 0.62f, 0.0f), vec3(0.08f, 0.07f, 0.08f), skin * 0.9f);            // neck
        part(head,  vec3(0.0f, 0.12f, 0.0f), vec3(0.22f, 0.23f, 0.22f), skin);                    // head
        part(head,  vec3(0.0f, 0.04f, 0.113f), vec3(0.10f, 0.035f, 0.02f), vec3(0.10f, 0.02f, 0.02f)); // open mouth
        if (hasHair) part(head, vec3(0.0f, 0.245f, -0.01f), vec3(0.235f, 0.06f, 0.20f), hair);
        if (id % 3 == 0) part(head, vec3(0.0f, 0.12f, -0.105f), vec3(0.235f, 0.20f, 0.05f), hair);

        // arms (shoulder joint), left sleeve torn short on some zombies
        bool torn = (id % 3 == 1);
        for (int i = 0; i < 2; ++i) {
            float ang = (i == 0) ? armL : armR;
            float side = (i == 0) ? -1.0f : 1.0f;
            glm::mat4 f = glm::rotate(glm::translate(upper, vec3(0.27f * side, 0.57f, 0.0f)), ang, vec3(1, 0, 0));
            f = glm::rotate(f, 0.12f * side, vec3(0, 0, 1));                                      // arms slightly out
            bool shortSleeve = torn && i == 0;
            part(f, vec3(0.0f, shortSleeve ? -0.08f : -0.15f, 0.0f), vec3(0.12f, shortSleeve ? 0.16f : 0.30f, 0.14f), shirt);
            part(f, vec3(0.0f, shortSleeve ? -0.32f : -0.41f, 0.0f), vec3(0.10f, shortSleeve ? 0.34f : 0.22f, 0.12f), skin);
        }

        // legs (hip joint)
        for (int i = 0; i < 2; ++i) {
            float ang = (i == 0) ? legL : legR;
            glm::mat4 f = glm::rotate(glm::translate(base, vec3(i == 0 ? -0.11f : 0.11f, 0.85f, 0.0f)), ang, vec3(1, 0, 0));
            part(f, vec3(0.0f, -0.38f, 0.0f), vec3(0.17f, 0.76f, 0.19f), pants);
            part(f, vec3(0.0f, -0.805f, 0.03f), vec3(0.18f, 0.09f, 0.28f), shoes);
        }

        // glowing eyes (the only self-lit part), skipped once dead
        if (z.state() != ZombieState::Dead) {
            shader.setFloat("uEmissive", 0.9f);
            part(head, vec3(-0.05f, 0.15f, 0.113f), vec3(0.04f, 0.03f, 0.012f), vec3(0.95f, 0.85f, 0.25f));
            part(head, vec3( 0.05f, 0.15f, 0.113f), vec3(0.04f, 0.03f, 0.012f), vec3(0.95f, 0.85f, 0.25f));
            shader.setFloat("uEmissive", 0.0f);
        }
    }
}
