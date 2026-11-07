// src/world/Sites.h
// Mission locations in world coordinates (metres). Level.cpp BUILDS props at these places and
// MissionManager.cpp uses the same numbers for interaction and objective checks, so they cannot disagree.
#pragma once
#include <glm/glm.hpp>

namespace sites {
    inline const glm::vec3 kClassroomDoor(3.75f, 1.3f, 4.1f);     // the way out of Objective 1
    inline const glm::vec3 kCard(-10.8f, 4.45f, 5.1f);            // access card on the table at the top of the stairs
    inline const glm::vec3 kCourtyard(3.7f, 1.2f, 16.0f);         // marker for "reach the courtyard"
    inline const glm::vec3 kPanel(-2.70f, 1.25f, 31.8f);          // access terminal inside the security booth
    inline const glm::vec3 kGate(3.6f, 1.2f, 33.4f);              // in front of the gate leaves
    inline const glm::vec3 kBeyondGate(3.6f, 1.5f, 36.5f);        // marker for "escape through the gate"
    inline const glm::vec3 kCheckpoint(3.6f, 0.0f, 40.0f);        // centre of the safe checkpoint on the road
    const float kCheckpointRadius = 1.8f;

    // Ammunition crates: classroom, corridor, staircase landing, security booth (centre of each crate, metres)
    inline const glm::vec3 kAmmo[4] = {
        glm::vec3(-4.30f, 0.35f, -3.20f),
        glm::vec3(-9.20f, 0.35f,  6.85f),
        glm::vec3(-18.4f, 1.95f,  4.60f),
        glm::vec3(-2.45f, 0.45f, 32.45f),
    };
}
