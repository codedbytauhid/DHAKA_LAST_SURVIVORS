// src/fx/ParticleSystem.h
// The DRAWING half of the particle system. All particles are drawn with ONE draw call:
// every frame the alive particles are written as camera-facing quads into one dynamic vertex buffer.
// Uses: VAO + VBO (dynamic) + EBO (static), a vertex and a fragment shader, alpha / additive blending.
#pragma once
#include <glm/glm.hpp>
#include "fx/ParticlePool.h"
#include "render/Shader.h"

class ParticleSystem {
public:
    ParticleSystem();                          // needs a current OpenGL context
    ~ParticleSystem();
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    bool isValid() const { return m_shader.isValid(); }
    ParticlePool& pool() { return m_pool; }
    void update(float dt) { m_pool.update(dt); }
    void clear() { m_pool.clear(); }

    // brightness: 1 outdoors, lower in dark rooms
    void draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, float brightness);

private:
    Shader m_shader;
    ParticlePool m_pool;
    unsigned m_vao = 0, m_vbo = 0, m_ebo = 0;
    std::vector<float> m_verts;                // rebuilt every frame (reserved once)
};
