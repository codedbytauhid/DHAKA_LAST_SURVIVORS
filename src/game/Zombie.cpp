// src/game/Zombie.cpp
#include "game/Zombie.h"
#include <algorithm>
#include <cmath>

using glm::vec3;

static const float kPi = 3.14159265f;
static float len2(float x, float z) { return std::sqrt(x * x + z * z); }
static float wrapAngle(float a)
{
    while (a >  kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}

Zombie::Zombie(int id, const vec3& feet, float wanderRadius, bool runner)
    : m_id(id), m_spawn(feet), m_home(feet), m_wanderRadius(std::max(1.0f, wanderRadius)), m_runner(runner)
{
    m_rng = 2463534242u + (unsigned int)id * 7919u;
    m_speedMul = 0.9f + 0.2f * rnd();
    m_limp = 0.5f + 0.5f * rnd();
    reset();
}

float Zombie::rnd()
{
    m_rng = m_rng * 1664525u + 1013904223u;
    return ((m_rng >> 8) & 0xFFFFu) / 65535.0f;
}

void Zombie::reset()
{
    m_body = CharacterBody();
    m_body.pos = vec3(m_spawn.x, m_spawn.y + 0.05f, m_spawn.z);   // settles onto the floor in the first frames
    m_body.radius = 0.30f;
    m_body.height = 1.75f;
    m_body.stepUp = 0.35f;
    m_home = m_spawn;
    m_facing = rnd() * 2.0f * kPi - kPi;
    m_state = ZombieState::Idle;
    m_stateTime = 0.0f;
    m_idleTimer = 1.0f + rnd() * 4.0f;
    m_lookTimer = 1.0f; m_headYaw = 0.0f; m_headTarget = 0.0f;
    m_groanTimer = 4.0f + rnd() * 8.0f;
    m_canSee = false; m_visionTimer = rnd() * 0.12f; m_lostFor = 0.0f;
    m_path.clear(); m_pathIdx = 0; m_replan = 0.0f;
    m_stuckTimer = 0.0f; m_stuckX = m_body.pos.x; m_stuckZ = m_body.pos.z; m_sidestep = 0.0f; m_stuckCount = 0;
    m_swinging = false; m_hitApplied = false; m_swingT = 0.0f; m_cooldown = 0.0f;
    m_phase = rnd() * 6.0f; m_time = rnd() * 10.0f; m_moveBlend = 0.0f; m_runBlend = 0.0f; m_deathT = 0.0f;
    m_health = m_maxHealth; m_hitTimer = 0.0f; m_alert = false;
}

void Zombie::kill()
{
    if (m_state == ZombieState::Dead) return;
    m_state = ZombieState::Dead;
    m_stateTime = 0.0f;
    m_deathT = 0.0f;
    m_swinging = false;
    m_body.vel.x = 0.0f;
    m_body.vel.z = 0.0f;
}

// ------------------------------------------------------------------ perception
bool Zombie::lineClear(const CollisionWorld& w, const vec3& a, const vec3& b, float h) const
{
    vec3 o(a.x, a.y + h, a.z), t(b.x, b.y + h, b.z);
    float dx = t.x - o.x, dy = t.y - o.y, dz = t.z - o.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist < 0.01f) return true;
    vec3 dir(dx / dist, dy / dist, dz / dist);
    return w.raycast(o, dir, dist) >= dist - 0.05f;      // nothing solid closer than the target
}

bool Zombie::perceive(const ZombieContext& c) const
{
    const ZombieConfig& cfg = *c.cfg;
    float dx = c.playerPos.x - m_body.pos.x, dz = c.playerPos.z - m_body.pos.z, dy = c.playerPos.y - m_body.pos.y;
    if (std::fabs(dy) > 2.6f) return false;                          // another floor
    float d = len2(dx, dz);
    bool tracking = (m_state == ZombieState::Chase || m_state == ZombieState::Attack);
    if (d > cfg.detectRange * (tracking ? 1.4f : 1.0f)) return false;

    bool near = d < cfg.senseRadius + c.playerSpeed * 0.8f;          // it "hears" you when you run
    bool inCone = false;
    if (d > 0.001f) inCone = ((std::sin(m_facing) * dx + std::cos(m_facing) * dz) / d) > cfg.fovCos;
    if (!(tracking || near || inCone)) return false;

    // line of sight at eye height: walls, lockers and the like block it, low furniture does not
    vec3 eye(m_body.pos.x, m_body.pos.y + cfg.eyeHeight, m_body.pos.z);
    vec3 chest(c.playerPos.x, c.playerPos.y + 1.25f, c.playerPos.z);
    float ex = chest.x - eye.x, ey = chest.y - eye.y, ez = chest.z - eye.z;
    float dist = std::sqrt(ex * ex + ey * ey + ez * ez);
    if (dist < 0.01f) return true;
    vec3 dir(ex / dist, ey / dist, ez / dist);
    return c.world->raycast(eye, dir, dist) >= dist - 0.25f;
}

// ------------------------------------------------------------------ movement
void Zombie::turnToward(float targetAngle, float rate, float dt)
{
    float d = wrapAngle(targetAngle - m_facing);
    m_facing = wrapAngle(m_facing + d * (1.0f - std::exp(-rate * dt)));
}

void Zombie::applyVelocity(float wx, float wz, float dt, const ZombieContext& c)
{
    wx += c.push.x;
    wz += c.push.y;
    float dx = wx - m_body.vel.x, dz = wz - m_body.vel.z;
    float maxChange = (m_body.grounded ? 14.0f : 5.0f) * dt;
    float dl = len2(dx, dz);
    if (dl > maxChange) { dx *= maxChange / dl; dz *= maxChange / dl; }
    m_body.vel.x += dx;
    m_body.vel.z += dz;
}

void Zombie::moveToward(const vec3& goal, float speed, float dt, const ZombieContext& c)
{
    if (m_hitTimer > 0.0f) speed *= 0.3f;           // staggering after being hit
    float dx = goal.x - m_body.pos.x, dz = goal.z - m_body.pos.z;
    float d = len2(dx, dz);
    float wx = 0.0f, wz = 0.0f;
    if (d > 0.05f) {
        float dirx = dx / d, dirz = dz / d;
        if (m_sidestep > 0.0f) {                       // stuck on something: slide off at an angle for a moment
            float a = m_sideSign * 1.15f;              // about 65 degrees
            float nx = dirx * std::cos(a) - dirz * std::sin(a);
            float nz = dirx * std::sin(a) + dirz * std::cos(a);
            dirx = nx; dirz = nz;
            m_sidestep -= dt;
        }
        wx = dirx * speed;
        wz = dirz * speed;
        turnToward(std::atan2(dirx, dirz), 7.0f, dt);

        // stuck detection: every half second, did we actually get anywhere?
        if (d > 0.6f) {
            m_stuckTimer += dt;
            if (m_stuckTimer >= 0.5f) {
                float moved = len2(m_body.pos.x - m_stuckX, m_body.pos.z - m_stuckZ);
                if (moved < speed * 0.5f * 0.25f) {
                    m_sidestep = 0.8f;
                    m_sideSign = (rnd() < 0.5f) ? -1.0f : 1.0f;
                    ++m_stuckCount;
                } else {
                    m_stuckCount = 0;
                }
                m_stuckTimer = 0.0f;
                m_stuckX = m_body.pos.x;
                m_stuckZ = m_body.pos.z;
            }
        }
    }
    applyVelocity(wx, wz, dt, c);
}

bool Zombie::pickWanderTarget(const ZombieContext& c)
{
    const CollisionWorld& w = *c.world;
    for (int tryNo = 0; tryNo < 8; ++tryNo) {
        float a = rnd() * 2.0f * kPi;
        float r = 0.8f + rnd() * (m_wanderRadius - 0.8f);
        float tx = m_home.x + std::cos(a) * r, tz = m_home.z + std::sin(a) * r;
        float g = w.groundHeightAt(tx, tz, m_body.pos.y + 0.4f, 0.3f);       // is there floor there?
        if (g < -1.0e8f || std::fabs(g - m_body.pos.y) > 0.4f) continue;     // no floor, or a different level
        if (!w.isClear(tx, g, tz, 0.35f, 1.75f, 0.35f)) continue;            // not inside furniture or a wall
        vec3 target(tx, g, tz);
        if (!lineClear(w, m_body.pos, target, 0.55f)) continue;              // nothing in the way
        m_wanderTarget = target;
        return true;
    }
    return false;
}

// Where should a chasing zombie that cannot see the player walk to? (straight there, or along the graph)
vec3 Zombie::chaseGoal(float dt, const ZombieContext& c)
{
    const CollisionWorld& w = *c.world;
    const NavGraph& nav = *c.nav;
    vec3 target = m_lastSeen;

    if (std::fabs(target.y - m_body.pos.y) < 0.6f && lineClear(w, m_body.pos, target, 0.55f)) {
        m_path.clear();
        return target;
    }

    m_replan -= dt;
    if (m_path.empty() || m_replan <= 0.0f || m_stuckCount >= 3) {
        m_replan = 1.0f;
        m_stuckCount = 0;
        std::vector<int> order = nav.sortedByDistance(m_body.pos);
        int start = order.empty() ? -1 : order[0];
        for (int i : order)                                     // first node we can walk to in a straight line
            if (std::fabs(nav.pos(i).y - m_body.pos.y) < 0.9f && lineClear(w, m_body.pos, nav.pos(i), 0.55f)) { start = i; break; }
        m_path = nav.findPath(start, nav.nearest(target));
        m_pathIdx = 0;
    }
    if (m_path.empty()) return target;

    // skip nodes we have reached, or can already walk past in a straight line
    while (m_pathIdx < m_path.size()) {
        const vec3& n = nav.pos(m_path[m_pathIdx]);
        bool reached = len2(n.x - m_body.pos.x, n.z - m_body.pos.z) < 0.6f && std::fabs(n.y - m_body.pos.y) < 1.0f;
        bool canSkip = false;
        if (!reached && m_pathIdx + 1 < m_path.size()) {
            const vec3& nn = nav.pos(m_path[m_pathIdx + 1]);
            canSkip = std::fabs(nn.y - m_body.pos.y) < 0.5f && lineClear(w, m_body.pos, nn, 0.55f);
        }
        if (reached || canSkip) ++m_pathIdx; else break;
    }
    if (m_pathIdx >= m_path.size()) return target;
    return nav.pos(m_path[m_pathIdx]);
}

// ------------------------------------------------------------------ state changes
void Zombie::enterIdle()
{
    m_state = ZombieState::Idle;
    m_stateTime = 0.0f;
    m_idleTimer = 2.0f + rnd() * 3.0f;
    m_stuckCount = 0;
}

void Zombie::enterWander(const ZombieContext& c)
{
    if (pickWanderTarget(c)) {
        m_state = ZombieState::Wander;
        m_stateTime = 0.0f;
        m_stuckCount = 0;
        m_stuckTimer = 0.0f; m_stuckX = m_body.pos.x; m_stuckZ = m_body.pos.z;
    } else {
        enterIdle();
    }
}

void Zombie::enterChase(const ZombieContext& c, ZombieOutput& out)
{
    if (m_state != ZombieState::Chase && m_state != ZombieState::Attack) out.detected = true;
    m_state = ZombieState::Chase;
    m_stateTime = 0.0f;
    m_lastSeen = c.playerPos;
    m_lostFor = 0.0f;
    m_path.clear(); m_pathIdx = 0; m_replan = 0.0f;
    m_headTarget = 0.0f;
    m_stuckCount = 0; m_sidestep = 0.0f;
    m_stuckTimer = 0.0f; m_stuckX = m_body.pos.x; m_stuckZ = m_body.pos.z;
}

void Zombie::enterAttack()
{
    m_state = ZombieState::Attack;
    m_stateTime = 0.0f;
    m_swinging = false;
    m_hitApplied = false;
    m_cooldown = 0.3f;          // a short wind-up before the first blow
}

// ------------------------------------------------------------------ the brain
ZombieOutput Zombie::update(float dt, const ZombieContext& c)
{
    ZombieOutput out;
    const ZombieConfig& cfg = *c.cfg;
    const CollisionWorld& world = *c.world;
    m_time += dt;
    m_stateTime += dt;

    if (m_state == ZombieState::Dead) {
        m_deathT = std::min(1.0f, m_deathT + dt / 1.1f);
        m_body.vel.x = 0.0f; m_body.vel.z = 0.0f;
        world.move(m_body, dt);                  // still obeys gravity, so it lands on the ground
        m_moveBlend = 0.0f;
        return out;
    }

    // combat reactions
    m_hitTimer = std::max(0.0f, m_hitTimer - dt);
    if (m_alert) {                                   // shot at, or heard a gunshot: turn on the player
        m_alert = false;
        if (c.playerAlive && (m_state == ZombieState::Idle || m_state == ZombieState::Wander)) enterChase(c, out);
    }

    // occasional groan (an audio hook for later)
    m_groanTimer -= dt;
    if (m_groanTimer <= 0.0f) { m_groanTimer = 6.0f + rnd() * 8.0f; out.groaned = true; }

    // vision is checked ~8 times a second, not every frame (ray tests are the expensive part)
    m_visionTimer -= dt;
    if (m_visionTimer <= 0.0f) {
        m_visionTimer = 0.10f + rnd() * 0.05f;
        m_canSee = c.playerAlive && perceive(c);
    }
    bool seen = m_canSee;

    float dx = c.playerPos.x - m_body.pos.x, dz = c.playerPos.z - m_body.pos.z, dy = c.playerPos.y - m_body.pos.y;
    float d = len2(dx, dz);

    switch (m_state) {
    case ZombieState::Idle: {
        applyVelocity(0.0f, 0.0f, dt, c);
        m_lookTimer -= dt;
        if (m_lookTimer <= 0.0f) { m_lookTimer = 1.5f + rnd() * 2.5f; m_headTarget = (rnd() - 0.5f) * 1.8f; }   // look around
        m_headYaw += (m_headTarget - m_headYaw) * (1.0f - std::exp(-4.0f * dt));
        if (seen) enterChase(c, out);
        else if (m_stateTime > m_idleTimer) enterWander(c);
        break;
    }
    case ZombieState::Wander: {
        if (seen) { enterChase(c, out); break; }
        m_headYaw += (0.0f - m_headYaw) * (1.0f - std::exp(-4.0f * dt));
        moveToward(m_wanderTarget, cfg.wanderSpeed * m_speedMul, dt, c);
        float wd = len2(m_wanderTarget.x - m_body.pos.x, m_wanderTarget.z - m_body.pos.z);
        if (wd < 0.5f || m_stateTime > 10.0f || m_stuckCount >= 2) enterIdle();
        break;
    }
    case ZombieState::Chase: {
        if (!c.playerAlive) { enterIdle(); break; }
        if (seen) { m_lastSeen = c.playerPos; m_lostFor = 0.0f; }
        else {
            m_lostFor += dt;
            if (m_lostFor < cfg.trackTime) m_lastSeen = c.playerPos;       // still following the sound of footsteps
        }
        if (m_lostFor > cfg.loseTime || d > cfg.loseRange) {               // gave up: wander around right here
            m_home = m_body.pos;
            enterWander(c);
            break;
        }
        if (seen && d <= cfg.attackRange * 0.9f && std::fabs(dy) < 1.3f) { enterAttack(); break; }

        float speed = (m_runner ? cfg.runnerSpeed : cfg.chaseSpeed) * m_speedMul;
        vec3 goal = seen ? c.playerPos : chaseGoal(dt, c);
        float gd = len2(goal.x - m_body.pos.x, goal.z - m_body.pos.z);
        if (!seen && gd < 0.8f && m_path.empty()) {                        // reached where it last saw you: look around
            applyVelocity(0.0f, 0.0f, dt, c);
            m_lookTimer -= dt;
            if (m_lookTimer <= 0.0f) { m_lookTimer = 1.0f; m_headTarget = (rnd() - 0.5f) * 2.0f; }
            m_headYaw += (m_headTarget - m_headYaw) * (1.0f - std::exp(-5.0f * dt));
        } else {
            m_headYaw += (0.0f - m_headYaw) * (1.0f - std::exp(-6.0f * dt));
            moveToward(goal, speed, dt, c);
        }
        break;
    }
    case ZombieState::Attack: {
        if (!c.playerAlive) { enterIdle(); break; }
        turnToward(std::atan2(dx, dz), 9.0f, dt);                          // face the player
        applyVelocity(0.0f, 0.0f, dt, c);
        if (seen) { m_lastSeen = c.playerPos; m_lostFor = 0.0f; } else m_lostFor += dt;

        if (!m_swinging) {
            m_cooldown -= dt;
            if (d > cfg.attackRange * 1.5f || std::fabs(dy) > 1.5f || m_lostFor > 1.0f) {
                m_state = ZombieState::Chase;                              // you stepped away: go after you again
                m_stateTime = 0.0f; m_path.clear(); m_pathIdx = 0; m_stuckCount = 0;
                break;
            }
            if (m_cooldown <= 0.0f && seen) {
                m_swinging = true; m_swingT = 0.0f; m_swingLen = cfg.swingTime; m_hitApplied = false;
                out.attacked = true;
            }
        } else {
            m_swingT += dt;
            if (!m_hitApplied && m_swingT >= cfg.hitTime) {                // the moment the blow lands
                m_hitApplied = true;
                float facingDot = (d > 0.001f) ? (std::sin(m_facing) * dx + std::cos(m_facing) * dz) / d : 1.0f;
                if (d <= cfg.attackRange * 1.35f && std::fabs(dy) < 1.4f && facingDot > 0.35f && seen)
                    out.damage = cfg.attackDamage;                          // only a blow that REACHES the player hurts
            }
            if (m_swingT >= m_swingLen) {
                m_swinging = false;
                m_cooldown = std::max(0.3f, cfg.attackCooldown - cfg.swingTime);
            }
        }
        break;
    }
    case ZombieState::Dead:
        break;
    }

    world.move(m_body, dt);
    if (m_body.pos.y < -30.0f) reset();

    // ---- animation numbers
    float spd = len2(m_body.vel.x, m_body.vel.z);
    float moving = (spd > 0.15f && m_body.grounded) ? 1.0f : 0.0f;
    float runT = std::max(0.0f, std::min(1.0f, (spd - 1.8f) / 2.2f));
    if (m_state == ZombieState::Chase && moving > 0.0f) runT = std::max(runT, 0.5f);
    float k = 1.0f - std::exp(-8.0f * dt);
    m_moveBlend += (moving - m_moveBlend) * k;
    m_runBlend  += (runT - m_runBlend) * k;
    m_phase += dt * (4.5f + spd * 2.2f);
    if (m_phase > 2000.0f) m_phase -= 2000.0f;
    return out;
}

// ------------------------------------------------------------------ combat
bool Zombie::rayHit(const vec3& o, const vec3& d, float maxDist, float& tOut, bool& head) const
{
    if (m_state == ZombieState::Dead) return false;         // bullets pass over corpses
    const vec3& p = m_body.pos;
    float fx = std::sin(m_facing) * 0.14f, fz = std::cos(m_facing) * 0.14f;   // the hunched head sits a little forward

    bool hit = false;
    float best = maxDist;
    float t = 0.0f;
    // head box first: it overlaps the top of the body box, and a head shot should win
    if (rayVsAABB(o, d, vec3(p.x + fx - 0.17f, p.y + 1.41f, p.z + fz - 0.17f),
                        vec3(p.x + fx + 0.17f, p.y + 1.75f, p.z + fz + 0.17f), t) && t < best) {
        best = t; head = true; hit = true;
    }
    if (rayVsAABB(o, d, vec3(p.x - 0.30f, p.y, p.z - 0.30f), vec3(p.x + 0.30f, p.y + 1.41f, p.z + 0.30f), t) && t < best) {
        best = t; head = false; hit = true;
    }
    if (hit) tOut = best;
    return hit;
}

bool Zombie::takeDamage(float amount)
{
    if (m_state == ZombieState::Dead) return false;
    m_health -= amount;
    m_hitTimer = 0.28f;
    if (m_health <= 0.0f) { m_health = 0.0f; kill(); return true; }
    if (m_state == ZombieState::Attack && m_swinging) {      // a hit interrupts the swing
        m_swinging = false;
        m_cooldown = 0.7f;
    }
    m_alert = true;
    return false;
}

void Zombie::hearNoise()
{
    if (m_state != ZombieState::Dead) m_alert = true;
}
