// src/world/Collision.cpp
#include "world/Collision.h"
#include <algorithm>
#include <cmath>

// Does the circle (cx,cz,r) touch the box when seen from above?
static bool circleTouchesBox(const AABB& b, float cx, float cz, float r)
{
    float dx = std::max(std::max(b.min.x - cx, 0.0f), cx - b.max.x);
    float dz = std::max(std::max(b.min.z - cz, 0.0f), cz - b.max.z);
    return dx * dx + dz * dz < r * r;
}

bool CollisionWorld::blockedAt(const CharacterBody& b, float nx, float nz) const
{
    float feet = b.pos.y, head = b.pos.y + b.height;
    for (const AABB& box : m_boxes) {
        if (!box.enabled) continue;
        if (box.max.y <= feet + b.stepUp + 0.001f) continue;   // low enough to step onto: not a wall
        if (box.min.y >= head) continue;                        // above our head: we walk under it
        if (!circleTouchesBox(box, nx, nz, b.radius)) continue; // not touching at the new position
        if (circleTouchesBox(box, b.pos.x, b.pos.z, b.radius)) continue; // already inside (e.g. after a teleport): let us leave
        return true;
    }
    return false;
}

void CollisionWorld::step(CharacterBody& b, float h, float gravity) const
{
    // ---- horizontal: X then Z, separately, so we slide along walls
    float nx = b.pos.x + b.vel.x * h;
    if (!blockedAt(b, nx, b.pos.z)) b.pos.x = nx; else b.vel.x = 0.0f;
    float nz = b.pos.z + b.vel.z * h;
    if (!blockedAt(b, b.pos.x, nz)) b.pos.z = nz; else b.vel.z = 0.0f;

    // ---- vertical
    bool wasGrounded = b.grounded;
    b.vel.y -= gravity * h;
    float ny = b.pos.y + b.vel.y * h;

    const float noGround = -1.0e9f;
    float ground = noGround, ceiling = 1.0e9f;
    float reach = b.pos.y + b.stepUp;
    float head = b.pos.y + b.height;
    for (const AABB& box : m_boxes) {
        if (!box.enabled) continue;
        if (!circleTouchesBox(box, b.pos.x, b.pos.z, b.radius * 0.9f)) continue;
        if (box.max.y <= reach)            ground = std::max(ground, box.max.y);       // a surface we can stand on
        else if (box.min.y >= head - 0.1f) ceiling = std::min(ceiling, box.min.y);     // something above our head
    }

    if (b.vel.y > 0.0f && ny + b.height > ceiling) {       // bumped our head
        ny = ceiling - b.height;
        b.vel.y = 0.0f;
    }

    if (ground > noGround && ny <= ground) {                // landed / standing
        ny = ground;
        if (b.vel.y < 0.0f) b.vel.y = 0.0f;
        b.grounded = true;
    } else if (wasGrounded && b.vel.y <= 0.0f && ground > noGround && b.pos.y - ground <= b.stepUp) {
        ny = ground;                                        // walking DOWN a step: stay glued to the stairs
        b.vel.y = 0.0f;
        b.grounded = true;
    } else {
        b.grounded = false;
    }
    b.pos.y = ny;
}

void CollisionWorld::move(CharacterBody& body, float dt, float gravity) const
{
    // Small sub-steps so a fast player can never jump over a thin wall in one frame.
    int n = std::max(1, (int)std::ceil(dt / (1.0f / 60.0f)));
    float h = dt / (float)n;
    for (int i = 0; i < n; ++i) step(body, h, gravity);
}

float CollisionWorld::raycast(const glm::vec3& o, const glm::vec3& d, float maxDist) const
{
    float best = maxDist;
    for (const AABB& b : m_boxes) {
        if (!b.enabled) continue;
        float tNear = -1.0e9f, tFar = 1.0e9f;
        bool miss = false;
        for (int a = 0; a < 3 && !miss; ++a) {
            if (std::fabs(d[a]) < 1.0e-8f) {
                if (o[a] < b.min[a] || o[a] > b.max[a]) miss = true;
            } else {
                float t1 = (b.min[a] - o[a]) / d[a];
                float t2 = (b.max[a] - o[a]) / d[a];
                if (t1 > t2) std::swap(t1, t2);
                tNear = std::max(tNear, t1);
                tFar  = std::min(tFar, t2);
                if (tNear > tFar) miss = true;
            }
        }
        if (miss || tFar < 0.0f || tNear < 0.0f) continue;   // missed, behind us, or we start inside it
        if (tNear < best) best = tNear;
    }
    return best;
}

float CollisionWorld::groundHeightAt(float x, float z, float maxY, float radius) const
{
    float best = -1.0e9f;
    for (const AABB& b : m_boxes)
        if (b.enabled && b.max.y <= maxY && circleTouchesBox(b, x, z, radius)) best = std::max(best, b.max.y);
    return best;
}

bool CollisionWorld::isClear(float x, float y, float z, float radius, float height, float stepUp) const
{
    for (const AABB& b : m_boxes) {
        if (!b.enabled) continue;
        if (b.max.y <= y + stepUp + 0.001f) continue;
        if (b.min.y >= y + height) continue;
        if (circleTouchesBox(b, x, z, radius)) return false;
    }
    return true;
}

bool rayVsAABB(const glm::vec3& o, const glm::vec3& d, const glm::vec3& mn, const glm::vec3& mx, float& tNearOut)
{
    float tNear = -1.0e9f, tFar = 1.0e9f;
    for (int a = 0; a < 3; ++a) {
        if (std::fabs(d[a]) < 1.0e-8f) {
            if (o[a] < mn[a] || o[a] > mx[a]) return false;
        } else {
            float t1 = (mn[a] - o[a]) / d[a];
            float t2 = (mx[a] - o[a]) / d[a];
            if (t1 > t2) std::swap(t1, t2);
            tNear = std::max(tNear, t1);
            tFar  = std::min(tFar, t2);
            if (tNear > tFar) return false;
        }
    }
    if (tFar < 0.0f) return false;           // the box is behind the ray
    tNearOut = std::max(0.0f, tNear);        // origin inside the box -> hit at distance 0
    return true;
}
