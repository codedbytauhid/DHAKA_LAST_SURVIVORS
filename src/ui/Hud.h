// src/ui/Hud.h
// A small 2D drawing toolkit: coloured rectangles and text made from the built-in pixel font.
// Everything is measured in PIXELS from the top-left corner of the window.
// The text size is given as "pixel size": how many screen pixels one pixel of the 5x7 font covers
// (2 = small text, 3 = normal, 6 = big title).
#pragma once
#include <string>
#include <glm/glm.hpp>
#include "render/Mesh.h"
#include "render/Shader.h"
#include "render/Texture.h"

class Hud {
public:
    Hud();                                         // needs a current OpenGL context
    bool isValid() const { return m_shader.isValid(); }

    void begin(int framebufferW, int framebufferH);   // switch to 2D drawing (no depth test, blending on)
    void end();                                       // switch back to 3D drawing

    int width() const { return m_w; }
    int height() const { return m_h; }

    void rect(float x, float y, float w, float h, const glm::vec4& rgba) const;
    void fullscreen(const glm::vec4& rgba) const { rect(0.0f, 0.0f, (float)m_w, (float)m_h, rgba); }

    float textWidth(const std::string& s, float px) const;
    void text(const std::string& s, float x, float y, float px, const glm::vec4& rgba) const;            // x, y = top-left
    void textShadow(const std::string& s, float x, float y, float px, const glm::vec4& rgba) const;      // with a dark outline-shadow
    void textCentered(const std::string& s, float centerX, float y, float px, const glm::vec4& rgba) const;

private:
    void quad(float x, float y, float w, float h) const;

    Shader m_shader;
    Mesh m_quad;
    Texture m_atlas;
    int m_w = 1, m_h = 1;
};
