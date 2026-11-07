// src/game/Zombie.h
// One zombie: its brain (a finite state machine), its body (a collision body like the player's)
// and the numbers the renderer needs for animation. Drawing lives in ZombieManager.cpp.
//
//   STATES                                 TRANSITIONS
//   Idle    stand, look around      Idle   -> Wander  after a few seconds
//   Wander  stroll near its home    Wander -> Idle    on arrival / when blocked
//   Chase   run at the player       Any    -> Chase   when it SEES the player
//   Attack  swing at the player     Chase  -> Attack  when close enough
//   Dead    lie on the ground       Attack -> Chase   when the player steps away
//                                   Chase  -> Wander  after losing the player for `loseTime` seconds
//                                   Any    -> Dead    when killed
#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "world/Collision.h"
#include "game/NavGraph.h"

enum class ZombieState { Idle, Wander, Chase, Attack, Dead };

// All the tuning numbers in one place.
struct ZombieConfig {
    float detectRange   = 13.0f;   // how far a zombie can SEE the player (inside its field of view)
    float fovCos        = 0.26f;   // cos(75 degrees): the zombie sees +-75 degrees around where it faces
    float senseRadius   = 3.5f;    // it also notices you all around when you are this close (plus your speed)
    float loseTime      = 5.0f;    // seconds without seeing you before it gives up
    float trackTime     = 2.0f;    // for this long after losing sight it still follows the sound of your steps
    float loseRange     = 24.0f;   // farther than this and it gives up at once
    float attackRange   = 1.15f;   // metres (centre to centre)
    float attackDamage  = 12.0f;
    float attackCooldown = 1.4f;   // seconds from the start of one swing to the start of the next
    float swingTime     = 0.7f;    // length of the swing animation
    float hitTime       = 0.35f;   // the blow lands this many seconds into the swing
    float wanderSpeed   = 0.9f;
    float chaseSpeed    = 2.5f;    // the player walks at 3.2 and sprints at 6.0, so you can always get away
    float runnerSpeed   = 3.8f;    // the few fast zombies
    float eyeHeight     = 1.5f;
    float health        = 100.0f;  // pistol: 34 per body shot, 68 per head shot
    float runnerHealth  = 80.0f;
};

struct ZombieContext {
    glm::vec3 playerPos{0.0f};     // player's feet
    float playerSpeed = 0.0f;      // louder when running
    bool playerAlive = true;
    glm::vec2 push{0.0f};          // separation from neighbouring zombies (set by the manager)
    const CollisionWorld* world = nullptr;
    const NavGraph* nav = nullptr;
    const ZombieConfig* cfg = nullptr;
};

struct ZombieOutput {              // what happened this frame (the manager turns these into events)
    float damage = 0.0f;           // damage that reached the player this frame
    bool detected = false;         // started chasing
    bool attacked = false;         // started a swing
    bool groaned = false;
};

class Zombie {
public:
    Zombie(int id, const glm::vec3& feet, float wanderRadius, bool runner);

    ZombieOutput update(float dt, const ZombieContext& ctx);
    void kill();                   // -> Dead (debug mode only; normal gameplay kills through takeDamage)

    // ---- combat
    // Ray against this zombie's hit boxes (a body box and a smaller head box). t = distance along the ray.
    bool rayHit(const glm::vec3& origin, const glm::vec3& dir, float maxDist, float& t, bool& head) const;
    // Returns true if this hit killed the zombie. A wounded zombie turns on the shooter and flinches.
    bool takeDamage(float amount);
    void hearNoise();              // a gunshot nearby: wake up and come looking
    void setMaxHealth(float h) { m_maxHealth = h; m_health = h; }
    float health() const { return m_health; }
    float maxHealth() const { return m_maxHealth; }
    float hitFlash() const { return m_hitTimer / 0.28f; }   // 1 right after being hit, fades to 0
    void reset();                  // back to the spawn point, alive and calm

    // --- read-only info for the renderer / HUD
    int id() const { return m_id; }
    ZombieState state() const { return m_state; }
    bool alive() const { return m_state != ZombieState::Dead; }
    const glm::vec3& position() const { return m_body.pos; }
    float facing() const { return m_facing; }
    float moveBlend() const { return m_moveBlend; }     // 0 standing .. 1 moving
    float runBlend() const { return m_runBlend; }       // 0 shuffle .. 1 fast
    float phase() const { return m_phase; }
    float time() const { return m_time; }
    float swingProgress() const { return m_swinging ? m_swingT / m_swingLen : -1.0f; }  // 0..1, or -1 when not swinging
    float deathProgress() const { return m_deathT; }    // 0..1
    float headYaw() const { return m_headYaw; }
    float limp() const { return m_limp; }
    bool runner() const { return m_runner; }

private:
    // behaviour helpers
    bool perceive(const ZombieContext& c) const;
    bool lineClear(const CollisionWorld& w, const glm::vec3& a, const glm::vec3& b, float h) const;
    bool pickWanderTarget(const ZombieContext& c);
    glm::vec3 chaseGoal(float dt, const ZombieContext& c);
    void moveToward(const glm::vec3& goal, float speed, float dt, const ZombieContext& c);
    void applyVelocity(float wx, float wz, float dt, const ZombieContext& c);
    void turnToward(float targetAngle, float rate, float dt);
    void enterIdle();
    void enterWander(const ZombieContext& c);
    void enterChase(const ZombieContext& c, ZombieOutput& out);
    void enterAttack();
    float rnd();                  // 0..1, per-zombie random numbers

    int m_id;
    glm::vec3 m_spawn, m_home;
    float m_wanderRadius;
    bool m_runner;
    unsigned int m_rng = 1u;

    CharacterBody m_body;
    ZombieState m_state = ZombieState::Idle;
    float m_stateTime = 0.0f;
    float m_facing = 0.0f;

    // idle / wander
    float m_idleTimer = 2.0f;
    glm::vec3 m_wanderTarget{0.0f};
    float m_lookTimer = 1.0f, m_headYaw = 0.0f, m_headTarget = 0.0f;
    float m_groanTimer = 8.0f;

    // perception
    float m_visionTimer = 0.0f;
    bool m_canSee = false;
    glm::vec3 m_lastSeen{0.0f};
    float m_lostFor = 0.0f;

    // navigation along the graph
    std::vector<int> m_path;
    size_t m_pathIdx = 0;
    float m_replan = 0.0f;

    // stuck handling
    float m_stuckTimer = 0.0f, m_stuckX = 0.0f, m_stuckZ = 0.0f;
    float m_sidestep = 0.0f, m_sideSign = 1.0f;
    int m_stuckCount = 0;

    // attack
    bool m_swinging = false, m_hitApplied = false;
    float m_swingT = 0.0f, m_swingLen = 0.7f, m_cooldown = 0.0f;

    // animation
    float m_phase = 0.0f, m_time = 0.0f, m_moveBlend = 0.0f, m_runBlend = 0.0f, m_deathT = 0.0f;
    float m_limp = 1.0f, m_speedMul = 1.0f;

    // combat
    float m_health = 100.0f, m_maxHealth = 100.0f, m_hitTimer = 0.0f;
    bool m_alert = false;          // shot at / heard a gunshot: switch to Chase on the next update
};
