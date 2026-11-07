// src/render/Mesh.h
// Wraps one VAO + VBO + EBO. Build it once, draw it many times.
#pragma once
#include <vector>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

class Mesh {
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;              // lets us return a Mesh from a function
    Mesh& operator=(Mesh&& other) noexcept;

    void draw() const;

    // Ready-made shapes (all built from vertices + indices).
    static Mesh createCube(float size);                       // centered at origin
    static Mesh createPlane(float size, float uvRepeat);      // flat, facing up (+Y)

private:
    unsigned int m_vao = 0, m_vbo = 0, m_ebo = 0;
    int m_indexCount = 0;
};