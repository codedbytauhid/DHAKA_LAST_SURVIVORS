// src/render/Camera.cpp
#include "render/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

glm::vec3 Camera::front() const
{
    float y = glm::radians(yaw);
    float p = glm::radians(pitch);
    return glm::normalize(glm::vec3(std::cos(y) * std::cos(p),
                                    std::sin(p),
                                    std::sin(y) * std::cos(p)));
}

glm::vec3 Camera::right() const
{
    return glm::normalize(glm::cross(front(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::mat4 Camera::view() const
{
    return glm::lookAt(position, position + front(), glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::rotate(float dx, float dy)
{
    yaw   += dx * sensitivity;
    pitch -= dy * sensitivity;           // screen Y grows downward, so subtract
    pitch = std::max(minPitch, std::min(maxPitch, pitch));
}

void Camera::snapTo(const glm::vec3& feetPos)
{
    m_pivot = feetPos + glm::vec3(0.0f, heightOffset, 0.0f);
    m_init = false;                      // update() will restart the distance too
    update(feetPos, 0.0f, CollisionWorld());
}

void Camera::update(const glm::vec3& feetPos, float dt, const CollisionWorld& world)
{
    glm::vec3 target = feetPos + glm::vec3(0.0f, heightOffset, 0.0f);

    // Smooth follow (exponential smoothing: frame-rate independent). Vertical is slower so stairs feel soft.
    if (!m_init) m_pivot = target;
    float kxz = 1.0f - std::exp(-14.0f * dt);
    float ky  = 1.0f - std::exp(-8.0f * dt);
    m_pivot.x += (target.x - m_pivot.x) * kxz;
    m_pivot.z += (target.z - m_pivot.z) * kxz;
    m_pivot.y += (target.y - m_pivot.y) * ky;

    distance = std::max(minDistance, std::min(maxDistance, distance));
    float dist = distance * (1.0f - 0.38f * aimBlend);            // aiming: closer ...
    float side = shoulder + 0.25f * aimBlend;                    // ... and more over the shoulder
    glm::vec3 wanted = m_pivot - front() * dist + right() * side;
    glm::vec3 d = wanted - m_pivot;
    float len = glm::length(d);
    glm::vec3 dir = d / len;

    // Collision: shorten the arm if a wall is in the way (keep a small margin).
    float hit = world.raycast(m_pivot, dir, len);
    float allowed = (hit < len) ? std::max(hit - 0.25f, 0.3f) : len;

    if (!m_init) { m_length = allowed; m_init = true; }
    else if (allowed < m_length) m_length = allowed;                            // wall: come in at once
    else m_length += (allowed - m_length) * (1.0f - std::exp(-5.0f * dt));      // free: ease back out

    position = m_pivot + dir * std::min(m_length, len);
}
