// src/game/Player.cpp
#include "game/Player.h"
#include "glad.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

static const float kPi = 3.14159265f;

static float wrapAngle(float a)
{
    while (a >  kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}

Player::Player()
    : m_cube(Mesh::createCube(1.0f)),
      m_cloth(Texture::fromFunction(32, 32, [](int x, int y) {      // subtle woven-cloth noise
          unsigned int h = ((unsigned int)x * 73856093u) ^ ((unsigned int)y * 19349663u);
          float g = 0.86f + 0.14f * ((h >> 4) & 255u) / 255.0f;
          return glm::vec3(g);
      }))
{
    m_body.radius = 0.28f;
    m_body.height = 1.75f;
    m_body.stepUp = 0.35f;
}

void Player::teleport(const glm::vec3& feetPos, float facingRad)
{
    m_body.pos = feetPos;
    m_body.vel = glm::vec3(0.0f);
    m_body.grounded = false;     // gravity will settle us onto the floor
    m_facing = facingRad;
    m_spawn = feetPos;
    m_spawnFacing = facingRad;
}

void Player::takeDamage(float amount)
{
    if (m_dead || amount <= 0.0f) return;
    m_health = std::max(0.0f, m_health - amount);
    m_hurt = 1.0f;
    if (m_health <= 0.0f) m_dead = true;
}

void Player::revive()
{
    m_health = maxHealth;
    m_dead = false;
    m_deadT = 0.0f;
    m_hurt = 0.0f;
}

void Player::update(float dt, const PlayerInput& inRaw, const glm::vec3& camF, const glm::vec3& camR,
                    const CollisionWorld& world)
{
    PlayerInput in = inRaw;
    if (m_dead) in = PlayerInput();          // a dead player cannot move
    m_hurt = std::max(0.0f, m_hurt - dt * 1.6f);
    if (m_dead) m_deadT = std::min(1.0f, m_deadT + dt / 1.0f);

    // 1. wanted direction, relative to the camera
    glm::vec3 dir = camF * in.forward + camR * in.right;
    dir.y = 0.0f;
    bool hasInput = glm::length(dir) > 0.001f;
    if (hasInput) dir = glm::normalize(dir);

    // 2. accelerate smoothly toward the wanted velocity (no instant starts and stops)
    float targetSpeed = hasInput ? (in.sprint ? sprintSpeed : walkSpeed) : 0.0f;
    if (in.aiming) targetSpeed = hasInput ? walkSpeed * 0.6f : 0.0f;     // careful, slower steps while aiming
    glm::vec2 vel(m_body.vel.x, m_body.vel.z);
    glm::vec2 want = glm::vec2(dir.x, dir.z) * targetSpeed;
    glm::vec2 diff = want - vel;
    float maxChange = (m_body.grounded ? accelGround : accelAir) * dt;
    float dl = glm::length(diff);
    if (dl > maxChange) diff *= maxChange / dl;
    vel += diff;
    m_body.vel.x = vel.x;
    m_body.vel.z = vel.y;

    // 3. turn the body smoothly: toward where the camera looks when aiming / shooting,
    //    otherwise toward the movement direction (shortest way round)
    m_aimPitch = in.aimPitch;
    if (in.faceCamera) {
        float d = wrapAngle(in.camYaw - m_facing);
        m_facing = wrapAngle(m_facing + d * (1.0f - std::exp(-18.0f * dt)));
    } else if (hasInput) {
        float target = std::atan2(dir.x, dir.z);
        float d = wrapAngle(target - m_facing);
        m_facing = wrapAngle(m_facing + d * (1.0f - std::exp(-turnRate * dt)));
    }

    // 4. jump
    if (in.jump && m_body.grounded) {
        m_body.vel.y = jumpSpeed;
        m_body.grounded = false;
        m_events.jumped = true;
    }

    // 5. gravity + collision + stairs
    bool wasGrounded = m_body.grounded;
    float fallSpeed = m_body.vel.y;
    world.move(m_body, dt);
    if (!wasGrounded && m_body.grounded && fallSpeed < -2.5f) m_events.landed = true;      // a real touch-down (not a tiny settle after a teleport)

    // fell out of the world? go back to the last teleport spot
    if (m_body.pos.y < -30.0f) teleport(m_spawn, m_spawnFacing);

    // 6. animation state
    float spd = speed();
    float moving = (spd > 0.3f && m_body.grounded) ? 1.0f : 0.0f;
    float runT = std::max(0.0f, std::min(1.0f, (spd - walkSpeed) / (sprintSpeed - walkSpeed)));
    float k = 1.0f - std::exp(-10.0f * dt);
    m_moveBlend += (moving - m_moveBlend) * k;
    m_runBlend  += (runT - m_runBlend) * k;
    m_phase += dt * (1.2f + spd * 1.55f);    // faster steps when moving faster (walk about 2 steps/s, sprint about 3.3)

    // a foot hits the ground every half cycle: that is the moment for a footstep sound
    long half = (long)std::floor(m_phase / kPi);
    if (half != m_lastHalfCycle) {
        m_lastHalfCycle = half;
        if (m_body.grounded && m_moveBlend > 0.4f && !m_dead) { m_events.step = true; m_events.run = spd > (walkSpeed + sprintSpeed) * 0.5f; }
    }
    m_time += dt;
    if (m_phase > kPi * 200.0f) { m_phase -= kPi * 200.0f; m_lastHalfCycle -= 200; }   // keep the number small (it is a long-lived float)
}

void Player::drawPart(const Shader& shader, const glm::mat4& frame, const glm::vec3& center,
                      const glm::vec3& size, const glm::vec3& color) const
{
    glm::mat4 m = glm::translate(frame, center);
    m = glm::scale(m, size);
    shader.setMat4("uModel", m);
    shader.setVec3("uTint", color);
    m_cube.draw();
}

glm::vec3 Player::muzzlePosition() const
{
    float c = std::cos(m_facing), s = std::sin(m_facing), cp = std::cos(m_aimPitch), sp = std::sin(m_aimPitch);
    glm::vec3 rightHand(-0.27f * c, 0.0f, 0.27f * s);             // the right hand is on the character's local -X side
    glm::vec3 aim(s * cp, sp, c * cp);
    return m_body.pos + glm::vec3(0.0f, 1.42f, 0.0f) + rightHand + aim * 0.78f;
}

void Player::draw(const Shader& shader, float sunScale, float ambientScale, const WeaponPose& pose) const
{
    // Shared material settings for the whole body (same uniforms the level uses)
    m_cloth.bind(0);
    shader.setVec2("uTexScale", glm::vec2(1.0f));
    shader.setInt("uUvMode", 1);
    shader.setInt("uMode", 0);
    shader.setFloat("uShininess", 16.0f);
    shader.setFloat("uSpecular", 0.05f);
    shader.setFloat("uEmissive", 0.0f);
    shader.setFloat("uAlpha", 1.0f);
    shader.setFloat("uSunScale", sunScale);
    shader.setFloat("uAmbientScale", ambientScale);

    const glm::vec3 skin(0.85f, 0.62f, 0.45f), hair(0.07f, 0.05f, 0.05f);
    const glm::vec3 shirt(0.30f, 0.55f, 0.85f), jeans(0.12f, 0.16f, 0.32f);
    const glm::vec3 shoes(0.85f), pack(0.90f, 0.40f, 0.10f), dark(0.03f);

    // ---- animation numbers
    float walkSwing = std::sin(m_phase) * (0.55f + 0.45f * m_runBlend) * m_moveBlend;   // radians
    float idleSway  = std::sin(m_time * 1.6f) * 0.04f * (1.0f - m_moveBlend);
    float lean      = (0.05f + 0.20f * m_runBlend) * m_moveBlend * (1.0f - pose.raise);   // lean forward when running (not when aiming)
    float bob       = std::fabs(std::sin(m_phase)) * 0.05f * m_moveBlend * (0.4f + 0.6f * m_runBlend);
    float breathe   = std::sin(m_time * 1.8f) * 0.006f * (1.0f - m_moveBlend);

    float legL = walkSwing, legR = -walkSwing, armL = -walkSwing * 0.9f + idleSway, armR = walkSwing * 0.9f - idleSway;
    if (!m_body.grounded) {                      // in the air: legs apart, arms up
        legL = 0.45f; legR = -0.25f; armL = -1.1f; armR = -1.1f;
    }

    // ---- frames (a "frame" is a matrix that moves everything attached to a joint)
    float fall = m_deadT * m_deadT * 1.5707963f;                     // death: topple forward onto the floor
    glm::mat4 base = glm::translate(glm::mat4(1.0f), m_body.pos + glm::vec3(0.0f, bob + 0.14f * m_deadT * m_deadT, 0.0f));
    base = glm::rotate(base, m_facing, glm::vec3(0, 1, 0));          // body faces local +Z
    base = glm::rotate(base, fall, glm::vec3(1, 0, 0));

    glm::mat4 upper = glm::translate(base, glm::vec3(0.0f, 0.85f, breathe));   // origin at the hips
    upper = glm::rotate(upper, lean - pose.recoil * 0.07f, glm::vec3(1, 0, 0));                    // the whole body rocks back a little when firing
    upper = glm::rotate(upper, std::sin(m_phase) * 0.11f * m_moveBlend * (1.0f - pose.raise), glm::vec3(0, 1, 0));   // shoulders twist against the hips
    upper = glm::rotate(upper, std::sin(m_phase + 1.57f) * 0.035f * m_moveBlend, glm::vec3(0, 0, 1));              // and sway

    // torso, backpack, neck, head, hair, eyes
    drawPart(shader, upper, glm::vec3(0.0f, 0.30f, 0.0f),   glm::vec3(0.42f, 0.60f, 0.22f), shirt);
    drawPart(shader, upper, glm::vec3(0.0f, 0.25f, -0.18f), glm::vec3(0.34f, 0.40f, 0.14f), pack);
    drawPart(shader, upper, glm::vec3(0.0f, 0.63f, 0.0f),   glm::vec3(0.08f, 0.06f, 0.08f), skin);
    drawPart(shader, upper, glm::vec3(0.0f, 0.74f, 0.0f),   glm::vec3(0.22f, 0.22f, 0.22f), skin);
    drawPart(shader, upper, glm::vec3(0.0f, 0.825f, 0.0f),  glm::vec3(0.24f, 0.07f, 0.24f), hair);
    drawPart(shader, upper, glm::vec3(0.0f, 0.76f, -0.095f), glm::vec3(0.24f, 0.20f, 0.05f), hair);
    drawPart(shader, upper, glm::vec3(-0.05f, 0.76f, 0.112f), glm::vec3(0.035f, 0.03f, 0.01f), dark);
    drawPart(shader, upper, glm::vec3( 0.05f, 0.76f, 0.112f), glm::vec3(0.035f, 0.03f, 0.01f), dark);

    // ---- arms. The character's RIGHT hand (local -X) holds the pistol.
    float raise = pose.raise;
    float aimAng = -(1.5707963f + m_aimPitch);                       // straight ahead, tilted up/down with the camera
    float rightArm = -0.80f + armL * 0.25f;                          // carried low in front of the body, with a little walking sway
    rightArm += (aimAng - rightArm) * raise;                         // raised when aiming or just after firing
    rightArm -= pose.recoil * 0.30f;                                 // recoil kicks the muzzle up
    float leftArm = armR * (1.0f - raise) + aimAng * 0.97f * raise;  // the other hand supports the pistol when raised
    float leftIn = 0.45f * raise;                                    // and swings toward the middle
    float roll = 0.0f;
    if (pose.reload >= 0.0f) {                                       // reload: lower the pistol, bring the other hand over, tilt it
        float k = std::sin(pose.reload * 3.14159265f);
        rightArm += (-0.7f - rightArm) * k;
        leftArm  += (-1.15f - leftArm) * k;
        leftIn   += 0.55f * k;
        roll = 0.9f * k;
    }
    glm::mat4 rightFrame = glm::translate(upper, glm::vec3(-0.27f, 0.57f, 0.0f));
    rightFrame = glm::rotate(rightFrame, rightArm, glm::vec3(1, 0, 0));
    rightFrame = glm::rotate(rightFrame, roll, glm::vec3(0, 1, 0));            // roll about the arm's own axis
    glm::mat4 leftFrame = glm::translate(upper, glm::vec3(0.27f, 0.57f, 0.0f));
    leftFrame = glm::rotate(leftFrame, leftArm, glm::vec3(1, 0, 0));
    leftFrame = glm::rotate(leftFrame, -leftIn, glm::vec3(0, 0, 1));
    for (int i = 0; i < 2; ++i) {
        const glm::mat4& f = (i == 0) ? rightFrame : leftFrame;
        drawPart(shader, f, glm::vec3(0.0f, -0.15f, 0.0f), glm::vec3(0.12f, 0.30f, 0.14f), shirt);   // sleeve
        drawPart(shader, f, glm::vec3(0.0f, -0.41f, 0.0f), glm::vec3(0.10f, 0.22f, 0.12f), skin);    // forearm + hand
    }

    // ---- the pistol, in the right hand. In the arm's own frame the barrel points along -Y and "up" is +Z.
    {
        const glm::mat4& f = rightFrame;
        const glm::vec3 metal(0.07f, 0.07f, 0.08f), gripCol(0.14f, 0.09f, 0.05f);
        shader.setFloat("uShininess", 64.0f);
        shader.setFloat("uSpecular", 0.6f);
        float slideBack = pose.recoil * 0.025f;                                                     // the slide jumps back when firing
        drawPart(shader, f, glm::vec3(0.0f, -0.60f + slideBack, 0.040f), glm::vec3(0.045f, 0.24f, 0.050f), metal);        // slide
        drawPart(shader, f, glm::vec3(0.0f, -0.58f, 0.008f),  glm::vec3(0.040f, 0.20f, 0.030f), metal * 1.5f);            // frame
        drawPart(shader, f, glm::vec3(0.0f, -0.525f, -0.040f), glm::vec3(0.040f, 0.050f, 0.100f), gripCol);               // grip
        drawPart(shader, f, glm::vec3(0.0f, -0.565f, -0.006f), glm::vec3(0.012f, 0.045f, 0.040f), metal * 1.5f);          // trigger guard
        // the magazine: it drops out, the hand fetches a new one, and it slides back in during a reload
        float rp = pose.reload, magDrop = 0.0f;
        bool magVisible = true;
        if (rp >= 0.0f) {
            if (rp >= 0.20f && rp < 0.45f) magDrop = -(rp - 0.20f) * 3.2f;           // falls away
            else if (rp >= 0.45f && rp < 0.62f) magVisible = false;                 // new one is being fetched
            else if (rp >= 0.62f && rp < 0.80f) magDrop = -(0.80f - rp) * 1.8f;      // slides up into the grip
        }
        if (magVisible) drawPart(shader, f, glm::vec3(0.0f, -0.525f, -0.105f + magDrop), glm::vec3(0.030f, 0.045f, 0.060f), metal * 1.3f);
        shader.setFloat("uShininess", 16.0f);
        shader.setFloat("uSpecular", 0.05f);
        if (pose.flash > 0.0f) {                                                                    // muzzle flash: a small glowing star
            shader.setFloat("uEmissive", 1.8f);
            drawPart(shader, f, glm::vec3(0.0f, -0.80f, 0.040f), glm::vec3(0.060f, 0.14f, 0.060f), glm::vec3(1.0f, 0.85f, 0.35f));
            drawPart(shader, f, glm::vec3(0.0f, -0.78f, 0.040f), glm::vec3(0.220f, 0.05f, 0.025f), glm::vec3(1.0f, 0.65f, 0.15f));
            drawPart(shader, f, glm::vec3(0.0f, -0.78f, 0.040f), glm::vec3(0.025f, 0.05f, 0.220f), glm::vec3(1.0f, 0.65f, 0.15f));
            shader.setFloat("uEmissive", 0.0f);
        }
    }

    // legs swing from the hips
    glm::mat4 legFrameL = glm::rotate(glm::translate(base, glm::vec3(-0.11f, 0.85f, 0.0f)), legL, glm::vec3(1, 0, 0));
    glm::mat4 legFrameR = glm::rotate(glm::translate(base, glm::vec3( 0.11f, 0.85f, 0.0f)), legR, glm::vec3(1, 0, 0));
    for (int i = 0; i < 2; ++i) {
        const glm::mat4& f = (i == 0) ? legFrameL : legFrameR;
        drawPart(shader, f, glm::vec3(0.0f, -0.38f, 0.0f),  glm::vec3(0.17f, 0.76f, 0.19f), jeans);
        drawPart(shader, f, glm::vec3(0.0f, -0.805f, 0.03f), glm::vec3(0.18f, 0.09f, 0.28f), shoes);
    }
}
