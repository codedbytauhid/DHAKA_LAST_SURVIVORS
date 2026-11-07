// src/game/ZombieManager.h
// Owns every zombie. Spawns them, updates them each frame, keeps them from stacking on top of each other,
// reports events (for future audio) and draws them all with ONE shared cube mesh and ONE texture.
#pragma once
#include <functional>
#include <vector>
#include <glm/glm.hpp>

#include "render/Mesh.h"
#include "render/Shader.h"
#include "render/Texture.h"
#include "world/Collision.h"
#include "world/Level.h"
#include "game/NavGraph.h"
#include "game/Zombie.h"

// Audio hooks: Milestone 6 will connect real sounds to these.
enum class ZombieEvent { Groan, Detect, Attack, Death };

class ZombieManager {
public:
    ZombieConfig config;                                            // tuning numbers shared by all zombies
    std::function<void(ZombieEvent, const glm::vec3&)> onEvent;     // optional: called when something happens

    ZombieManager();                                                // needs a current OpenGL context

    // Creates up to `count` zombies from the placement table (12 entries, most important first).
    void spawn(int count, const CollisionWorld& world);
    void reset();                                                   // everyone back to their spawn point, alive

    // Returns the damage that reached the player this frame (usually 0).
    float update(float dt, const glm::vec3& playerPos, float playerSpeed, bool playerAlive, const CollisionWorld& world);

    void draw(const Shader& shader, const Level& level, const glm::vec3& cameraPos) const;

    // Gunshot noise: every living zombie within `radius` metres (walls do not matter) wakes up and comes looking.
    void alertNoise(const glm::vec3& at, float radius);

    // The zombies themselves (the pistol's shot test needs to look at them and hurt them).
    std::vector<Zombie>& zombies() { return m_zombies; }
    void announceDeath(const glm::vec3& at) const { emit(ZombieEvent::Death, at); }

    // DEBUG MODE ONLY (key K, only reachable after F1): kills the closest living zombie within maxDist.
    bool killNearest(const glm::vec3& from, float maxDist);

    int total() const { return (int)m_zombies.size(); }
    int alive() const;
    int countState(ZombieState s) const;

private:
    void emit(ZombieEvent e, const glm::vec3& at) const { if (onEvent) onEvent(e, at); }

    Mesh m_cube;
    Texture m_cloth;
    NavGraph m_nav;
    std::vector<Zombie> m_zombies;
};
