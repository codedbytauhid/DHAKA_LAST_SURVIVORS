// src/game/Combat.h
// Hitscan shooting: no flying bullets, we simply trace a line and see what it hits first.
//
// In a third-person game the camera is NOT where the gun is, so we do it in two steps:
//   1. AIM: trace a line from the camera through the crosshair. The first thing it touches (a wall or a
//      zombie) is the AIM POINT: "where the player is pointing".
//   2. BULLET: trace a second line from the player's chest to that aim point and on. THIS line is the real
//      bullet. A wall between the player and the zombie stops it, so you cannot shoot through walls
//      even when the camera can see the zombie.
#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "world/Collision.h"
#include "game/Zombie.h"

struct ShotResult {
    bool hitZombie = false;
    int zombieIndex = -1;
    bool head = false;
    bool killed = false;
    float damage = 0.0f;
    bool hitWorld = false;          // the bullet ended on a wall / floor / object
    glm::vec3 start{0.0f};          // where the bullet started (the player's chest)
    glm::vec3 impact{0.0f};         // where it ended
    glm::vec3 dir{0.0f, 0.0f, 1.0f};
};

// camOrigin / camDir: the camera ray through the crosshair. bulletStart: the player's chest.
// Applies the damage to the zombie that was hit (and kills it if its health runs out).
ShotResult resolveShot(const glm::vec3& camOrigin, const glm::vec3& camDir, const glm::vec3& bulletStart,
                       float range, float damage, float headMultiplier,
                       const CollisionWorld& world, std::vector<Zombie>& zombies);
