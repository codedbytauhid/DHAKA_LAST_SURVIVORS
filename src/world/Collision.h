// src/world/Collision.h
// Lightweight collision for a walking character.
//
// HOW IT WORKS (beginner version)
//  * Every solid object in the level gets an AABB: an "axis-aligned bounding box" = the smallest
//    box (no rotation) that contains it. It is just two corners: min (x,y,z) and max (x,y,z).
//  * The player is a vertical cylinder: a circle of `radius` on the floor and `height` tall.
//  * Each frame we try to move the player along X, then along Z. If the new position would push the
//    circle into a box that is in the way, we cancel THAT axis only. Because the axes are tested
//    separately, the player "slides" along walls instead of sticking to them.
//  * A box is only "in the way" if it is taller than `stepUp` (0.35 m) above the feet and starts below
//    the head. Low things (floor slabs, stair steps 0.2 m high) are NOT in the way: the player
//    simply stands on them. That one rule is the whole staircase solution.
//  * Vertical movement: gravity pulls down; we find the highest surface under the feet
//    ("ground") and the lowest surface above the head ("ceiling") and clamp to them.
#pragma once
#include <vector>
#include <glm/glm.hpp>

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
    bool enabled = true;      // a disabled box is ignored by every query (used for the opening gate)
};

struct CharacterBody {
    glm::vec3 pos{0.0f};      // position of the FEET (bottom centre)
    glm::vec3 vel{0.0f};      // metres per second
    float radius = 0.28f;
    float height = 1.75f;
    float stepUp = 0.35f;     // highest ledge we can walk up without jumping
    bool grounded = false;
};

// Ray against ONE box. Returns true if the ray hits it; tNear = distance along the ray (0 if the origin is inside).
bool rayVsAABB(const glm::vec3& origin, const glm::vec3& dir, const glm::vec3& boxMin, const glm::vec3& boxMax, float& tNear);

class CollisionWorld {
public:
    int add(const AABB& box) { m_boxes.push_back(box); return (int)m_boxes.size() - 1; }   // returns the box id
    void setEnabled(int id, bool enabled) { if (id >= 0 && id < (int)m_boxes.size()) m_boxes[id].enabled = enabled; }
    size_t count() const { return m_boxes.size(); }

    // Moves the body by body.vel * dt with gravity, collision, sliding, steps and ground snapping.
    void move(CharacterBody& body, float dt, float gravity = 16.0f) const;

    // Highest surface under a circle at (x,z) that is not above maxY. Returns -1e9 if there is none.
    float groundHeightAt(float x, float z, float maxY, float radius) const;

    // True if a body standing with its feet at (x,y,z) would not be inside any wall or large object.
    bool isClear(float x, float y, float z, float radius, float height, float stepUp) const;

    // Distance from `origin` along the unit vector `dir` to the first solid box (or maxDist if none).
    // The camera uses this so it does not go through walls.
    float raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist) const;

private:
    void step(CharacterBody& b, float h, float gravity) const;
    bool blockedAt(const CharacterBody& b, float nx, float nz) const;

    std::vector<AABB> m_boxes;
};
