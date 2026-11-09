// src/fx/ParticlePool.cpp
#include "fx/ParticlePool.h"
#include <cmath>

using glm::vec3;
using glm::vec4;

ParticlePool::ParticlePool(size_t capacity) : m_cap(capacity)
{
    m_p.reserve(capacity);          // allocate once: nothing is allocated while playing
}

float ParticlePool::rand01()
{
    m_rng ^= m_rng << 13; m_rng ^= m_rng >> 17; m_rng ^= m_rng << 5;
    return (m_rng & 0xFFFFFF) / 16777216.0f;
}

bool ParticlePool::emit(const Particle& p)
{
    if (m_p.size() >= m_cap) return false;
    m_p.push_back(p);
    return true;
}

void ParticlePool::update(float dt)
{
    for (size_t i = 0; i < m_p.size();) {
        Particle& p = m_p[i];
        p.age += dt;
        if (p.age >= p.life) {                       // expired: replace it with the last particle and look at this slot again
            p = m_p.back();
            m_p.pop_back();
            continue;
        }
        p.vel.y -= p.gravity * dt;
        float damp = std::max(0.0f, 1.0f - p.drag * dt);
        p.vel.x *= damp; p.vel.y *= damp; p.vel.z *= damp;
        p.pos.x += p.vel.x * dt; p.pos.y += p.vel.y * dt; p.pos.z += p.vel.z * dt;
        ++i;
    }
}

// a random unit-ish vector for scattering
static vec3 scatter(ParticlePool& pool, float amount)
{
    return vec3(pool.randRange(-amount, amount), pool.randRange(-amount, amount), pool.randRange(-amount, amount));
}

void ParticlePool::blood(const vec3& at, const vec3& dir, bool killed)
{
    int drops = killed ? 22 : 10;
    for (int i = 0; i < drops; ++i) {                // droplets spray back out of the wound and fall
        Particle p;
        p.pos = at;
        vec3 sp = scatter(*this, 1.4f);
        float back = randRange(0.5f, 3.0f);
        p.vel = vec3(-dir.x * back + sp.x, -dir.y * back + sp.y + 1.2f, -dir.z * back + sp.z);
        p.color = vec4(randRange(0.35f, 0.55f), 0.02f, 0.03f, 0.95f);
        p.life = randRange(0.4f, 0.9f);
        p.size0 = randRange(0.035f, 0.07f); p.size1 = p.size0 * 0.8f;
        p.gravity = 9.0f; p.drag = 0.8f;
        emit(p);
    }
    for (int i = 0; i < (killed ? 3 : 1); ++i) {     // a faint red mist
        Particle p;
        p.pos = at;
        p.vel = vec3(-dir.x * 0.6f, 0.3f, -dir.z * 0.6f) + scatter(*this, 0.3f);
        p.color = vec4(0.5f, 0.03f, 0.04f, 0.35f);
        p.life = 0.55f;
        p.size0 = 0.15f; p.size1 = 0.5f;
        p.drag = 2.0f;
        emit(p);
    }
}

void ParticlePool::bloodSplat(const vec3& at)
{
    for (int i = 0; i < 8; ++i) {
        Particle p;
        p.pos = at + vec3(randRange(-0.2f, 0.2f), randRange(0.2f, 1.0f), randRange(-0.2f, 0.2f));
        p.vel = scatter(*this, 0.6f);
        p.color = vec4(0.4f, 0.02f, 0.03f, 0.9f);
        p.life = randRange(0.4f, 0.8f);
        p.size0 = 0.05f; p.size1 = 0.04f;
        p.gravity = 9.0f;
        emit(p);
    }
}

void ParticlePool::wallImpact(const vec3& at, const vec3& dir)
{
    for (int i = 0; i < 4; ++i) {                    // grey dust puffs that drift outwards and thin out
        Particle p;
        p.pos = at;
        p.vel = vec3(-dir.x * 0.8f, 0.4f, -dir.z * 0.8f) + scatter(*this, 0.5f);
        p.color = vec4(0.70f, 0.67f, 0.60f, 0.45f);
        p.life = randRange(0.5f, 0.9f);
        p.size0 = 0.08f; p.size1 = randRange(0.3f, 0.5f);
        p.drag = 2.5f;
        emit(p);
    }
    for (int i = 0; i < 7; ++i) {                    // bright sparks (additive)
        Particle p;
        p.pos = at;
        p.vel = vec3(-dir.x * randRange(1.0f, 4.0f), randRange(0.5f, 3.0f), -dir.z * randRange(1.0f, 4.0f)) + scatter(*this, 2.0f);
        p.color = vec4(1.0f, randRange(0.6f, 0.85f), 0.25f, 1.0f);
        p.life = randRange(0.12f, 0.3f);
        p.size0 = 0.035f; p.size1 = 0.01f;
        p.gravity = 8.0f; p.glow = 1.0f;
        emit(p);
    }
    for (int i = 0; i < 2; ++i) {                    // tiny chips of debris
        Particle p;
        p.pos = at;
        p.vel = vec3(-dir.x * randRange(0.5f, 2.0f), randRange(1.0f, 2.5f), -dir.z * randRange(0.5f, 2.0f)) + scatter(*this, 0.8f);
        p.color = vec4(0.35f, 0.33f, 0.30f, 1.0f);
        p.life = randRange(0.4f, 0.7f);
        p.size0 = 0.03f; p.size1 = 0.03f;
        p.gravity = 12.0f;
        emit(p);
    }
}

void ParticlePool::muzzleFlash(const vec3& at, const vec3& dir)
{
    Particle p;                                      // the big bright flash: very short
    p.pos = at;
    p.vel = vec3(dir.x, dir.y, dir.z) * 1.0f;
    p.color = vec4(1.0f, 0.78f, 0.35f, 1.0f);
    p.life = 0.055f;
    p.size0 = 0.34f; p.size1 = 0.12f;
    p.glow = 1.0f;
    emit(p);
    for (int i = 0; i < 5; ++i) {                    // small streaks of fire thrown forward
        Particle s;
        s.pos = at;
        s.vel = vec3(dir.x, dir.y, dir.z) * randRange(4.0f, 9.0f) + scatter(*this, 1.5f);
        s.color = vec4(1.0f, 0.65f, 0.2f, 1.0f);
        s.life = randRange(0.05f, 0.12f);
        s.size0 = 0.05f; s.size1 = 0.01f;
        s.glow = 1.0f;
        emit(s);
    }
}

void ParticlePool::muzzleSmoke(const vec3& at)
{
    for (int i = 0; i < 2; ++i) {
        Particle p;
        p.pos = at;
        p.vel = vec3(randRange(-0.1f, 0.1f), randRange(0.25f, 0.5f), randRange(-0.1f, 0.1f));
        p.color = vec4(0.6f, 0.6f, 0.62f, 0.22f);
        p.life = randRange(0.7f, 1.1f);
        p.size0 = 0.05f; p.size1 = 0.28f;
        p.drag = 0.8f;
        emit(p);
    }
}

void ParticlePool::smokePuff(const vec3& at, float windX)
{
    Particle p;
    p.pos = at + vec3(randRange(-0.3f, 0.3f), 0.0f, randRange(-0.3f, 0.3f));
    p.vel = vec3(windX + randRange(-0.2f, 0.2f), randRange(0.7f, 1.2f), randRange(-0.2f, 0.2f));
    float g = randRange(0.15f, 0.3f);
    p.color = vec4(g, g, g * 1.05f, 0.5f);
    p.life = randRange(6.0f, 8.0f);
    p.size0 = 0.8f; p.size1 = randRange(3.0f, 4.5f);
    p.drag = 0.15f;
    emit(p);
}

void ParticlePool::dustMote(const vec3& at, bool indoor)
{
    Particle p;
    p.pos = at;
    p.vel = vec3(randRange(-0.05f, 0.05f) + (indoor ? 0.0f : 0.5f), randRange(-0.02f, 0.05f), randRange(-0.05f, 0.05f));
    p.color = indoor ? vec4(0.95f, 0.88f, 0.72f, 0.5f) : vec4(0.85f, 0.7f, 0.55f, 0.4f);
    p.life = randRange(4.0f, 7.0f);
    p.size0 = p.size1 = indoor ? 0.018f : 0.03f;
    emit(p);
}
