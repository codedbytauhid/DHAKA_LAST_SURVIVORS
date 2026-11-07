// src/game/Combat.cpp
#include "game/Combat.h"
#include <cmath>

using glm::vec3;

namespace {
// nearest living zombie along a ray (hit boxes), closer than maxDist
int nearestZombie(const vec3& o, const vec3& d, float maxDist, const std::vector<Zombie>& zs, float& tOut, bool& headOut)
{
    int best = -1;
    float bestT = maxDist;
    for (size_t i = 0; i < zs.size(); ++i) {
        float t; bool head = false;
        if (zs[i].rayHit(o, d, bestT, t, head) && t < bestT) { bestT = t; best = (int)i; headOut = head; }
    }
    if (best >= 0) tOut = bestT;
    return best;
}
}

ShotResult resolveShot(const vec3& camOrigin, const vec3& camDir, const vec3& bulletStart, float range,
                       float damage, float headMultiplier, const CollisionWorld& world, std::vector<Zombie>& zombies)
{
    ShotResult r;
    r.start = bulletStart;

    // ---- 1. AIM: what is under the crosshair?
    float wallT = world.raycast(camOrigin, camDir, range);
    float t = 0.0f; bool h = false;
    int aimed = nearestZombie(camOrigin, camDir, wallT, zombies, t, h);
    float aimDist = (aimed >= 0) ? t : wallT;
    vec3 aimPoint(camOrigin.x + camDir.x * aimDist, camOrigin.y + camDir.y * aimDist, camOrigin.z + camDir.z * aimDist);

    // ---- 2. BULLET: from the chest through the aim point
    float dx = aimPoint.x - bulletStart.x, dy = aimPoint.y - bulletStart.y, dz = aimPoint.z - bulletStart.z;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    vec3 dir = (len > 0.05f) ? vec3(dx / len, dy / len, dz / len) : camDir;
    r.dir = dir;

    float wallB = world.raycast(bulletStart, dir, range);                 // first wall along the bullet's path
    float zt = 0.0f; bool head = false;
    int zi = nearestZombie(bulletStart, dir, wallB, zombies, zt, head);   // only zombies in FRONT of that wall count

    if (zi >= 0) {
        r.hitZombie = true;
        r.zombieIndex = zi;
        r.head = head;
        r.damage = damage * (head ? headMultiplier : 1.0f);
        r.impact = vec3(bulletStart.x + dir.x * zt, bulletStart.y + dir.y * zt, bulletStart.z + dir.z * zt);
        r.killed = zombies[zi].takeDamage(r.damage);
    } else {
        r.hitWorld = wallB < range;
        r.impact = vec3(bulletStart.x + dir.x * wallB, bulletStart.y + dir.y * wallB, bulletStart.z + dir.z * wallB);
    }
    return r;
}
