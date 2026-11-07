// src/game/ShotEffects.cpp
#include "game/ShotEffects.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

static const unsigned char kWhitePixel[3] = { 255, 255, 255 };

ShotEffects::ShotEffects() : m_cube(Mesh::createCube(1.0f)), m_white(1, 1, 3, kWhitePixel) {}

void ShotEffects::addShot(const glm::vec3& muzzle, const glm::vec3& impact, bool hitZombie)
{
    if (m_tracers.size() > 16) m_tracers.erase(m_tracers.begin());
    if (m_sparks.size() > 16) m_sparks.erase(m_sparks.begin());
    m_tracers.push_back({ muzzle, impact, 0.07f });
    m_sparks.push_back({ impact, hitZombie ? glm::vec3(0.85f, 0.05f, 0.05f) : glm::vec3(1.0f, 0.75f, 0.25f), 0.18f });
}

void ShotEffects::update(float dt)
{
    for (Tracer& t : m_tracers) t.life -= dt;
    for (Spark& s : m_sparks) s.life -= dt;
    m_tracers.erase(std::remove_if(m_tracers.begin(), m_tracers.end(), [](const Tracer& t) { return t.life <= 0.0f; }), m_tracers.end());
    m_sparks.erase(std::remove_if(m_sparks.begin(), m_sparks.end(), [](const Spark& s) { return s.life <= 0.0f; }), m_sparks.end());
}

void ShotEffects::clear() { m_tracers.clear(); m_sparks.clear(); }

void ShotEffects::draw(const Shader& shader) const
{
    if (m_tracers.empty() && m_sparks.empty()) return;
    m_white.bind(0);
    shader.setVec2("uTexScale", glm::vec2(1.0f));
    shader.setInt("uUvMode", 1);
    shader.setInt("uMode", 0);
    shader.setFloat("uAlpha", 1.0f);
    shader.setFloat("uShininess", 8.0f);
    shader.setFloat("uSpecular", 0.0f);
    shader.setFloat("uSunScale", 0.0f);        // lit only by their own glow
    shader.setFloat("uAmbientScale", 0.0f);
    shader.setFloat("uEmissive", 2.0f);

    for (const Tracer& t : m_tracers) {
        glm::vec3 d = t.b - t.a;
        float len = glm::length(d);
        if (len < 0.05f) continue;
        d /= len;
        float yaw = std::atan2(d.x, d.z);
        float pitch = -std::asin(std::max(-1.0f, std::min(1.0f, d.y)));
        float fade = t.life / 0.07f;
        glm::mat4 m = glm::translate(glm::mat4(1.0f), (t.a + t.b) * 0.5f);
        m = glm::rotate(m, yaw, glm::vec3(0, 1, 0));
        m = glm::rotate(m, pitch, glm::vec3(1, 0, 0));
        m = glm::scale(m, glm::vec3(0.012f + 0.01f * fade, 0.012f + 0.01f * fade, len));
        shader.setMat4("uModel", m);
        shader.setVec3("uTint", glm::vec3(1.0f, 0.9f, 0.5f));
        m_cube.draw();
    }
    for (const Spark& s : m_sparks) {
        float size = 0.06f + 0.10f * (s.life / 0.18f);
        glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.0f), s.pos), glm::vec3(size));
        shader.setMat4("uModel", m);
        shader.setVec3("uTint", s.color);
        m_cube.draw();
    }
    shader.setFloat("uEmissive", 0.0f);
}
