// src/fx/ParticlePool.h
// The SIMULATION half of the particle system (no OpenGL here, so it is easy to test).
// A particle is a tiny billboard with a position, velocity, colour and size that changes over its life.
// All particles live in one pre-sized array; dead ones are removed by swapping with the last one, so no
// memory is allocated while playing. A hard cap keeps integrated graphics happy.
#pragma once
#include <vector>
#include <glm/glm.hpp>

struct Particle {
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    glm::vec4 color{1.0f};        // rgb + base alpha
    float age = 0.0f, life = 1.0f;
    float size0 = 0.1f, size1 = 0.1f;   // size at birth and at death (metres)
    float gravity = 0.0f;         // downward acceleration
    float drag = 0.0f;            // slows the particle down (per second)
    float glow = 0.0f;            // 0 = normal blending (smoke, blood, dust), 1 = additive (sparks, flash)
};

class ParticlePool {
public:
    explicit ParticlePool(size_t capacity = 700);

    void clear() { m_p.clear(); }
    void update(float dt);                        // move, age and remove expired particles
    const std::vector<Particle>& particles() const { return m_p; }
    size_t count() const { return m_p.size(); }
    size_t capacity() const { return m_cap; }

    bool emit(const Particle& p);                 // false (and nothing happens) if the pool is full

    // ---- ready-made effects. `dir` = the direction the bullet was travelling (unit vector).
    void blood(const glm::vec3& at, const glm::vec3& dir, bool killed);
    void wallImpact(const glm::vec3& at, const glm::vec3& dir);
    void muzzleFlash(const glm::vec3& at, const glm::vec3& dir);
    void muzzleSmoke(const glm::vec3& at);
    void smokePuff(const glm::vec3& at, float windX);                         // one puff of a smoke column
    void dustMote(const glm::vec3& at, bool indoor);                          // a floating speck
    void bloodSplat(const glm::vec3& at);                                     // a few drops falling from a dead zombie

    float rand01();                               // 0..1 (the pool's own random numbers)
    float randRange(float a, float b) { return a + (b - a) * rand01(); }

private:
    std::vector<Particle> m_p;
    size_t m_cap;
    unsigned m_rng = 2463534242u;
};
