// src/game/Player.h
// The playable student: movement, collision body, animation, and drawing.
// The body is built from the same cube Mesh the level uses: head, torso, two arms, two legs,
// backpack, hair and shoes, each one a scaled cube. Arms and legs swing from their joints.
#pragma once
#include <glm/glm.hpp>
#include "render/Mesh.h"
#include "render/Shader.h"
#include "render/Texture.h"
#include "world/Collision.h"
#include "game/Weapon.h"

struct PlayerInput {
    float forward = 0.0f;    // +1 = W, -1 = S
    float right   = 0.0f;    // +1 = D, -1 = A
    bool sprint = false;
    bool jump   = false;
    // ---- pistol (Milestone 6)
    bool aiming = false;           // right mouse button held: slower movement, arms up
    bool faceCamera = false;       // aiming or just fired: the body turns to face where the camera looks
    float camYaw = 0.0f;           // direction the camera looks, as atan2(x, z) (radians)
    float aimPitch = 0.0f;         // camera pitch in radians (positive = looking up), tilts the raised arm
};

class Player {
public:
    // Tunable numbers (metres, seconds)
    float walkSpeed   = 3.2f;
    float sprintSpeed = 6.0f;
    float jumpSpeed   = 5.6f;
    float accelGround = 24.0f;   // how quickly we reach the wanted speed on the ground
    float accelAir    = 7.0f;    // less control in the air
    float turnRate    = 14.0f;   // how quickly the body turns toward the movement direction

    Player();                    // needs a current OpenGL context

    // camForward / camRight: the camera's directions flattened onto the ground (unit length),
    // so W always means "away from the camera".
    void update(float dt, const PlayerInput& in, const glm::vec3& camForward, const glm::vec3& camRight,
                const CollisionWorld& world);

    // facingRad: direction the body faces, as atan2(dirX, dirZ). Also remembers this as the respawn point.
    void teleport(const glm::vec3& feetPos, float facingRad);

    void draw(const Shader& shader, float sunScale, float ambientScale, const WeaponPose& pose) const;

    // where a bullet starts (the chest: always inside the player's own space, so it never starts inside a wall)
    glm::vec3 chestPosition() const { return m_body.pos + glm::vec3(0.0f, 1.35f, 0.0f); }
    // where the pistol's muzzle is, for the tracer line and muzzle flash
    glm::vec3 muzzlePosition() const;
    float facing() const { return m_facing; }

    // ---- health (Milestone 4)
    float maxHealth = 100.0f;
    float health() const { return m_health; }
    bool dead() const { return m_dead; }
    float hurtFlash() const { return m_hurt; }        // 1 right after being hit, fades to 0
    void takeDamage(float amount);                    // never goes below 0; at 0 the player dies
    void revive();                                    // full health, standing up again (used by restart)

    const glm::vec3& position() const { return m_body.pos; }
    bool grounded() const { return m_body.grounded; }
    float speed() const { return glm::length(glm::vec2(m_body.vel.x, m_body.vel.z)); }

private:
    void drawPart(const Shader& shader, const glm::mat4& frame, const glm::vec3& center,
                  const glm::vec3& size, const glm::vec3& color) const;

    Mesh m_cube;
    Texture m_cloth;
    CharacterBody m_body;

    float m_facing = 0.0f;       // radians
    float m_aimPitch = 0.0f;     // radians, from the camera
    glm::vec3 m_spawn{0.0f};
    float m_spawnFacing = 0.0f;

    float m_health = 100.0f;
    bool m_dead = false;
    float m_deadT = 0.0f;        // 0 standing .. 1 lying on the ground
    float m_hurt = 0.0f;

    // animation state
    float m_phase = 0.0f;        // walk cycle angle
    float m_time = 0.0f;         // for idle breathing
    float m_moveBlend = 0.0f;    // 0 idle .. 1 moving
    float m_runBlend = 0.0f;     // 0 walk .. 1 sprint
};
