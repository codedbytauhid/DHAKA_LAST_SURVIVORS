// src/render/Camera.h
// Third-person orbit camera.
//  * yaw / pitch = the direction the camera LOOKS (the mouse changes them).
//  * Each frame update() places the camera behind the player's head along that direction.
//  * A ray is cast from the head to the wanted camera spot; if a wall is in the way the camera
//    moves closer to the player, so it never ends up inside geometry.
#pragma once
#include <glm/glm.hpp>
#include "world/Collision.h"

class Camera {
public:
    glm::vec3 position{0.0f, 1.7f, 3.0f};
    float yaw   = -90.0f;     // degrees. -90 = looking toward -Z
    float pitch = -12.0f;     // degrees. Positive = looking up
    float sensitivity = 0.10f;

    // Third-person settings (all adjustable)
    float distance     = 3.6f;    // metres behind the head (mouse wheel changes it)
    float heightOffset = 1.55f;   // pivot height above the feet (about head height)
    float shoulder     = 0.30f;   // shifts the camera to the right so the player is left of centre
    float minPitch = -55.0f, maxPitch = 35.0f;
    float minDistance = 1.5f, maxDistance = 7.0f;
    float aimBlend = 0.0f;        // 0..1 set by main from the weapon: pulls the camera closer and over the shoulder

    glm::vec3 front() const;   // unit vector the camera looks along
    glm::vec3 right() const;   // unit vector pointing to the camera's right
    glm::mat4 view() const;

    void rotate(float dx, float dy);   // dx, dy = mouse movement in pixels
    void snapTo(const glm::vec3& feetPos);                                   // jump without smoothing (teleport)
    void update(const glm::vec3& feetPos, float dt, const CollisionWorld& world);

private:
    glm::vec3 m_pivot{0.0f};
    float m_length = 0.0f;     // current distance from pivot to camera (after collision)
    bool m_init = false;
};
