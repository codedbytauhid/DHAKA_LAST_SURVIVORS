// src/render/Shader.cpp
#include "render/Shader.h"
#include "glad.h"
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

// Read a whole text file into a string. Returns false if it cannot be opened.
static bool readFile(const std::string& path, std::string& out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::fprintf(stderr, "[Shader] Cannot open file: %s\n"
                             "        (run the game from the project root folder)\n", path.c_str());
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

// Compile one shader stage. Returns 0 on failure and prints the GLSL error log.
static unsigned int compileStage(unsigned int type, const std::string& source, const std::string& label)
{
    unsigned int id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        int len = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetShaderInfoLog(id, (int)log.size(), nullptr, log.data());
        std::fprintf(stderr, "[Shader] Compile error in %s:\n%s\n", label.c_str(), log.data());
        glDeleteShader(id);
        return 0;
    }
    return id;
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    std::string vsSource, fsSource;
    if (!readFile(vertexPath, vsSource) || !readFile(fragmentPath, fsSource))
        return;

    unsigned int vs = compileStage(GL_VERTEX_SHADER, vsSource, vertexPath);
    unsigned int fs = compileStage(GL_FRAGMENT_SHADER, fsSource, fragmentPath);
    if (vs == 0 || fs == 0) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // The stages are copied into the program, so we can delete them now.
    glDeleteShader(vs);
    glDeleteShader(fs);

    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        int len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetProgramInfoLog(program, (int)log.size(), nullptr, log.data());
        std::fprintf(stderr, "[Shader] Link error:\n%s\n", log.data());
        glDeleteProgram(program);
        return;
    }
    m_id = program;
}

Shader::~Shader()
{
    if (m_id) glDeleteProgram(m_id);
}

void Shader::use() const
{
    glUseProgram(m_id);
}

int Shader::location(const std::string& name) const
{
    auto it = m_locations.find(name);
    if (it != m_locations.end()) return it->second;
    int loc = glGetUniformLocation(m_id, name.c_str());
    m_locations[name] = loc;
    return loc;
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(location(name), value);
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(location(name), value);
}

void Shader::setVec2(const std::string& name, const glm::vec2& v) const
{
    glUniform2f(location(name), v.x, v.y);
}

void Shader::setVec4(const std::string& name, const glm::vec4& v) const
{
    glUniform4f(location(name), v.x, v.y, v.z, v.w);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const
{
    glUniform3f(location(name), x, y, z);
}

void Shader::setVec3(const std::string& name, const glm::vec3& v) const
{
    glUniform3f(location(name), v.x, v.y, v.z);
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const
{
    glUniformMatrix4fv(location(name), 1, GL_FALSE, glm::value_ptr(value));
}
