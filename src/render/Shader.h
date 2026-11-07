// src/render/Shader.h
// Loads, compiles and links a vertex + fragment shader pair.
// RAII: the GPU program is deleted automatically in the destructor.
// Milestone 2 change: uniform locations are cached (the level now draws hundreds of
// objects per frame, so we avoid asking OpenGL for the same location again and again).
#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

class Shader {
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;             // a GPU program id must not be copied
    Shader& operator=(const Shader&) = delete;

    bool isValid() const { return m_id != 0; }
    void use() const;

    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const glm::vec2& v) const;   // NEW
    void setVec4(const std::string& name, const glm::vec4& v) const;   // NEW (HUD colours / rectangles)
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setVec3(const std::string& name, const glm::vec3& v) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;

private:
    int location(const std::string& name) const;   // cached glGetUniformLocation

    unsigned int m_id = 0;
    mutable std::unordered_map<std::string, int> m_locations;
};
