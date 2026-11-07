// src/render/Mesh.cpp
#include "render/Mesh.h"
#include "glad.h"

#include <cstddef>   // offsetof
#include <utility>

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    : m_indexCount((int)indices.size())
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    // VBO: the vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // EBO: the index list (which vertices form each triangle). Stored inside the VAO.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Attribute layout: 0 = position, 1 = normal, 2 = uv
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_vao(other.m_vao), m_vbo(other.m_vbo), m_ebo(other.m_ebo), m_indexCount(other.m_indexCount)
{
    other.m_vao = other.m_vbo = other.m_ebo = 0;
    other.m_indexCount = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other) {
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
        if (m_ebo) glDeleteBuffers(1, &m_ebo);
        m_vao = other.m_vao; m_vbo = other.m_vbo; m_ebo = other.m_ebo;
        m_indexCount = other.m_indexCount;
        other.m_vao = other.m_vbo = other.m_ebo = 0;
        other.m_indexCount = 0;
    }
    return *this;
}

void Mesh::draw() const
{
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

Mesh Mesh::createCube(float size)
{
    const float h = size * 0.5f;

    // For each face: normal, and two directions (u, v) along the face.
    // u x v = normal, which makes the triangles counter-clockwise when seen from outside.
    struct Face { glm::vec3 n, u, v; };
    const Face faces[6] = {
        {{ 0, 0, 1}, { 1, 0, 0}, {0, 1,  0}},   // front  (+Z)
        {{ 0, 0,-1}, {-1, 0, 0}, {0, 1,  0}},   // back   (-Z)
        {{ 1, 0, 0}, { 0, 0,-1}, {0, 1,  0}},   // right  (+X)
        {{-1, 0, 0}, { 0, 0, 1}, {0, 1,  0}},   // left   (-X)
        {{ 0, 1, 0}, { 1, 0, 0}, {0, 0, -1}},   // top    (+Y)
        {{ 0,-1, 0}, { 1, 0, 0}, {0, 0,  1}},   // bottom (-Y)
    };

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (int f = 0; f < 6; ++f) {
        const Face& face = faces[f];
        unsigned int base = (unsigned int)vertices.size();

        vertices.push_back({ (face.n - face.u - face.v) * h, face.n, {0, 0} });
        vertices.push_back({ (face.n + face.u - face.v) * h, face.n, {1, 0} });
        vertices.push_back({ (face.n + face.u + face.v) * h, face.n, {1, 1} });
        vertices.push_back({ (face.n - face.u + face.v) * h, face.n, {0, 1} });

        // Two triangles per face, reusing the 4 vertices through indices.
        indices.insert(indices.end(), { base, base + 1, base + 2,  base, base + 2, base + 3 });
    }
    return Mesh(vertices, indices);
}

Mesh Mesh::createPlane(float size, float uvRepeat)
{
    const float h = size * 0.5f;
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    std::vector<Vertex> vertices = {
        {{-h, 0.0f,  h}, up, {0.0f,     0.0f}},
        {{ h, 0.0f,  h}, up, {uvRepeat, 0.0f}},
        {{ h, 0.0f, -h}, up, {uvRepeat, uvRepeat}},
        {{-h, 0.0f, -h}, up, {0.0f,     uvRepeat}},
    };
    std::vector<unsigned int> indices = { 0, 1, 2,  0, 2, 3 };
    return Mesh(vertices, indices);
}