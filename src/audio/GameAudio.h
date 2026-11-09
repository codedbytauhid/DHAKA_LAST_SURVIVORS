// src/audio/GameAudio.h
// The "sound director": translates game events (a footstep, a zombie noticing you, the gate unlocking, the game
// state changing ...) into calls on the AudioManager. It decides WHICH sound, how loud, how varied, and which
// ambience / music should play where. It has no OpenGL code.
#pragma once
#include <functional>
#include <vector>
#include <glm/glm.hpp>
#include "audio/AudioManager.h"
#include "game/Mission.h"        // GameState, MissionEvent
#include "game/Zombie.h"         // ZombieEvent

struct FlickerProbe {            // a broken light that may buzz when it blinks
    glm::vec3 pos;
    float intensity;
};

class GameAudio {
public:
    explicit GameAudio(AudioManager& audio) : m_a(audio) {}

    // Set by main: returns true if a solid wall lies between the two points (zombies behind walls sound muffled).
    std::function<bool(const glm::vec3&, const glm::vec3&)> isBlocked;

    void stateChanged(GameState from, GameState to);      // music / ambience / pause handling
    void update(float dt, GameState state, const glm::vec3& listenerPos, const glm::vec3& listenerRight,
                bool indoors, float introTime, const std::vector<FlickerProbe>& flicker);

    // player
    void footstep(bool run, bool indoors);
    void jump();
    void land();
    void pistolShot();
    void pistolEmpty();
    void pistolReload();
    void bulletImpact(const glm::vec3& at);
    void playerHurt();
    void playerDeath();
    // world
    void zombieEvent(ZombieEvent e, const glm::vec3& at);
    void missionEvent(MissionEvent e);
    // interface
    void uiSelect();
    void uiConfirm();

private:
    float rnd();                                           // 0..1
    float between(float a, float b) { return a + (b - a) * rnd(); }
    float distanceToListener(const glm::vec3& p) const;
    void startAmbience();
    void stopAmbience(float fade);
    void setAmbience(float indoor, float wind, float siren, float fade);

    AudioManager& m_a;
    glm::vec3 m_listener{0.0f}, m_right{1.0f, 0.0f, 0.0f};
    int m_amb[3] = { -1, -1, -1 };                        // loop handles: indoor, wind, siren
    float m_ambLast[3] = { -1, -1, -1 };
    int m_music = -1;
    float m_groanCooldown = 0.0f, m_indoorBlend = 0.0f;
    bool m_broadcastDone = false, m_leftFoot = false;
    std::vector<float> m_prevFlicker;
    unsigned m_rng = 88172645u;
};
