// src/fx/ParticleSystem.cpp
#include "fx/ParticleSystem.h"
#include "glad.h"

#include <algorithm>
#include <cstddef>
#include <vector>

static const int kFloatsPerVertex = 10;       // pos(3) uv(2) color(4) glow(1)

ParticleSystem::ParticleSystem()
    : m_shader("shaders/particle.vert", "shaders/particle.frag"), m_pool(700)
{
    const size_t cap = m_pool.capacity();
    m_verts.reserve(cap * 4 * kFloatsPerVertex);

    // static index buffer: quad i uses vertices 4i..4i+3  ->  two triangles
    std::vector<unsigned int> idx(cap * 6);
    for (size_t i = 0; i < cap; ++i) {
        unsigned int b = (unsigned int)(i * 4);
        unsigned int q[6] = { b, b + 1, b + 2, b, b + 2, b + 3 };
        for (int k = 0; k < 6; ++k) idx[i * 6 + k] = q[k];
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, cap * 4 * kFloatsPerVertex * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    const GLsizei stride = kFloatsPerVertex * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                      glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));    glEnableVertexAttribArray(3);
    glBindVertexArray(0);
}

ParticleSystem::~ParticleSystem()
{
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
}

void ParticleSystem::draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, float brightness)
{
    const std::vector<Particle>& ps = m_pool.particles();
    if (ps.empty() || !m_shader.isValid()) return;

    // the camera's right and up directions (they are the first two rows of the view matrix): quads always face the camera
    glm::vec3 right(view[0][0], view[1][0], view[2][0]);
    glm::vec3 up(view[0][1], view[1][1], view[2][1]);

    m_verts.clear();
    int quads = 0;
    for (const Particle& p : ps) {
        glm::vec3 d = p.pos - cameraPos;
        if (d.x * d.x + d.y * d.y + d.z * d.z > 70.0f * 70.0f) continue;          // too far to matter
        float u = p.age / p.life;
        float size = (p.size0 + (p.size1 - p.size0) * u) * 0.5f;
        float fadeIn = std::min(1.0f, u / 0.08f);
        float a = p.color.a * fadeIn * (1.0f - u);
        glm::vec3 r = right * size, t = up * size;
        const glm::vec3 corner[4] = { p.pos - r - t, p.pos + r - t, p.pos + r + t, p.pos - r + t };
        const float uv[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
        for (int k = 0; k < 4; ++k) {
            float v[kFloatsPerVertex] = { corner[k].x, corner[k].y, corner[k].z, uv[k][0], uv[k][1],
                                          p.color.r, p.color.g, p.color.b, a, p.glow };
            m_verts.insert(m_verts.end(), v, v + kFloatsPerVertex);
        }
        ++quads;
    }
    if (quads == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_verts.size() * sizeof(float), m_verts.data());

    m_shader.use();
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProjection", projection);
    m_shader.setFloat("uBrightness", brightness);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);        // premultiplied alpha (see particle.frag)
    glDepthMask(GL_FALSE);                              // particles are seen behind walls, but never hide each other or the world
    glDisable(GL_CULL_FACE);
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, quads * 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
