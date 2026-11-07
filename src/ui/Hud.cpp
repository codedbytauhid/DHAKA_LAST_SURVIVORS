// src/ui/Hud.cpp
#include "ui/Hud.h"
#include "ui/Font.h"
#include "glad.h"

#include <algorithm>
#include <vector>

static Mesh makeQuad()
{
    std::vector<Vertex> v = {
        {{0.0f, 0.0f, 0.0f}, {0, 0, 1}, {0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}, {0, 0, 1}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, 0.0f}, {0, 0, 1}, {1.0f, 1.0f}},
        {{0.0f, 1.0f, 0.0f}, {0, 0, 1}, {0.0f, 1.0f}},
    };
    std::vector<unsigned int> idx = { 0, 1, 2, 0, 2, 3 };
    return Mesh(v, idx);
}

Hud::Hud()
    : m_shader("shaders/hud.vert", "shaders/hud.frag"),
      m_quad(makeQuad()),
      m_atlas(Font::createAtlas())
{
}

void Hud::begin(int w, int h)
{
    m_w = w; m_h = h;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_shader.use();
    m_shader.setInt("uTex", 0);
}

void Hud::end()
{
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

// pixels (top-left origin) -> normalized device coordinates (-1..1, y up)
void Hud::quad(float x, float y, float w, float h) const
{
    float nw = w / m_w * 2.0f;
    float nh = h / m_h * 2.0f;
    float nx = x / m_w * 2.0f - 1.0f;
    float ny = 1.0f - y / m_h * 2.0f - nh;           // bottom edge
    m_shader.setVec4("uRect", glm::vec4(nx, ny, nw, nh));
    m_quad.draw();
}

void Hud::rect(float x, float y, float w, float h, const glm::vec4& c) const
{
    m_shader.setVec4("uUV", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    m_shader.setVec4("uColor", c);
    m_shader.setInt("uUseTex", 0);
    quad(x, y, w, h);
}

float Hud::textWidth(const std::string& s, float px) const
{
    if (s.empty()) return 0.0f;
    return ((float)s.size() * 6.0f - 1.0f) * px;
}

void Hud::text(const std::string& s, float x, float y, float px, const glm::vec4& c) const
{
    m_atlas.bind(0);
    m_shader.setVec4("uColor", c);
    m_shader.setInt("uUseTex", 1);
    const float cell = (float)Font::kCell / (float)(Font::kCell * Font::kAtlasCols);   // one cell, as a fraction of the atlas
    float cx = x;
    for (char ch : s) {
        int gi = Font::glyphIndex(ch);
        if (gi >= 0) {
            float u = (float)(gi % Font::kAtlasCols) * cell;
            float v = (float)(gi / Font::kAtlasCols) * cell;
            m_shader.setVec4("uUV", glm::vec4(u, v, cell, cell));
            quad(cx, y, 8.0f * px, 8.0f * px);        // a glyph cell is 8x8 font pixels: 5 wide, 7 tall (+1 row of padding below)
        }
        cx += 6.0f * px;
    }
}

void Hud::textShadow(const std::string& s, float x, float y, float px, const glm::vec4& c) const
{
    float o = std::max(1.0f, px * 0.5f);
    text(s, x + o, y + o, px, glm::vec4(0.0f, 0.0f, 0.0f, c.a * 0.8f));
    text(s, x, y, px, c);
}

void Hud::textCentered(const std::string& s, float centerX, float y, float px, const glm::vec4& c) const
{
    textShadow(s, centerX - textWidth(s, px) * 0.5f, y, px, c);
}
