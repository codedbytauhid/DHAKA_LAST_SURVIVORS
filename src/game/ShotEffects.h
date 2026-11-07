// src/game/ShotEffects.h
// The short-lived visuals of a gunshot: a bright tracer line from the muzzle to where the bullet ended and a
// small spark where it hit (yellow on walls, red on zombies). Drawn with the same lit shader as everything else,
// as self-lit cubes. They live a fraction of a second.
#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "render/Mesh.h"
#include "render/Shader.h"
#include "render/Texture.h"

class ShotEffects {
public:
    ShotEffects();                                   // needs a current OpenGL context
    void addShot(const glm::vec3& muzzle, const glm::vec3& impact, bool hitZombie);
    void update(float dt);
    void clear();
    void draw(const Shader& shader) const;           // call after the world has been drawn (shader already in use)

private:
    struct Tracer { glm::vec3 a, b; float life; };
    struct Spark  { glm::vec3 pos, color; float life; };
    Mesh m_cube;
    Texture m_white;
    std::vector<Tracer> m_tracers;
    std::vector<Spark> m_sparks;
};
