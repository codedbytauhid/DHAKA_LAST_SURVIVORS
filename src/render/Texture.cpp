// src/render/Texture.cpp
#include "render/Texture.h"
#include "glad.h"

#include <cmath>
#include <vector>

// ---------- tiny deterministic "random" number from two ints, result 0..1 ----------
static float hash2(int x, int y)
{
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return (h & 0xFFFFu) / 65535.0f;
}

// ---------- the OpenGL part ----------
Texture::Texture(int width, int height, int channels, const unsigned char* pixels)
{
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);   // our rows are tightly packed
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);         // smaller copies -> less shimmering far away

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);   // tile when UV > 1
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::~Texture()
{
    if (m_id) glDeleteTextures(1, &m_id);
}

Texture::Texture(Texture&& other) noexcept : m_id(other.m_id) { other.m_id = 0; }

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other) {
        if (m_id) glDeleteTextures(1, &m_id);
        m_id = other.m_id;
        other.m_id = 0;
    }
    return *this;
}

void Texture::bind(unsigned int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::setNearest() const
{
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture Texture::fromFunction(int w, int h, const std::function<glm::vec3(int, int)>& fn)
{
    std::vector<unsigned char> data((size_t)w * h * 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            glm::vec3 c = glm::clamp(fn(x, y), 0.0f, 1.0f);
            size_t i = ((size_t)y * w + x) * 3;
            data[i + 0] = (unsigned char)(c.r * 255.0f);
            data[i + 1] = (unsigned char)(c.g * 255.0f);
            data[i + 2] = (unsigned char)(c.b * 255.0f);
        }
    }
    return Texture(w, h, 3, data.data());
}

// ---------- procedural pictures ----------
Texture Texture::createBrick()
{
    return fromFunction(128, 128, [](int x, int y) {
        const int brickW = 32, brickH = 16;
        int row = y / brickH;
        int xo  = (x + (row % 2) * (brickW / 2)) % 128;   // every second row shifted by half a brick
        int bx  = xo % brickW;
        int by  = y % brickH;
        float grain = 0.9f + 0.2f * hash2(x, y);
        if (bx < 2 || by < 2)                              // mortar (the grey lines)
            return glm::vec3(0.62f, 0.60f, 0.56f) * grain;
        float tone = 0.75f + 0.5f * hash2(xo / brickW, row);   // each brick slightly different
        return glm::vec3(0.55f, 0.24f, 0.18f) * tone * grain;
    });
}

Texture Texture::createConcrete()
{
    return fromFunction(128, 128, [](int x, int y) {
        float blotch = hash2(x / 8, y / 8);
        float grain  = hash2(x, y);
        float g = 0.50f + 0.12f * blotch + 0.08f * grain;
        return glm::vec3(g, g, g * 0.97f);
    });
}

Texture Texture::createWoodFloor()
{
    return fromFunction(128, 128, [](int x, int y) {
        int plank = x / 16;
        if (x % 16 == 0 || (y + plank * 37) % 64 == 0)     // dark gaps between planks
            return glm::vec3(0.12f, 0.07f, 0.04f);
        float tone  = 0.8f + 0.4f * hash2(plank, 3);
        float grain = 0.92f + 0.08f * std::sin(y * 0.35f + plank * 5.0f) + 0.06f * hash2(x, y);
        return glm::vec3(0.45f, 0.28f, 0.14f) * tone * grain;
    });
}

Texture Texture::createGrass()
{
    return fromFunction(128, 128, [](int x, int y) {
        float coarse = hash2(x / 4, y / 4);
        float fine   = hash2(x, y);
        float t = 0.65f + 0.25f * coarse + 0.2f * fine;
        return glm::vec3(0.18f, 0.40f, 0.12f) * t;
    });
}

Texture Texture::createBlackboard()
{
    return fromFunction(128, 128, [](int x, int y) {
        float smudge = hash2(x / 6, y / 10);
        float g = 0.10f + 0.06f * smudge + 0.02f * hash2(x, y);
        return glm::vec3(g * 0.6f, g * 1.4f, g);
    });
}

Texture Texture::createChecker()
{
    return fromFunction(128, 128, [](int x, int y) {
        bool light = ((x / 16) + (y / 16)) % 2 == 0;
        return light ? glm::vec3(0.8f) : glm::vec3(0.25f);
    });
}
