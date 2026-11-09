// src/audio/GameAudio.cpp
#include "audio/GameAudio.h"
#include "world/Sites.h"
#include <algorithm>
#include <cmath>

float GameAudio::rnd()
{
    m_rng ^= m_rng << 13; m_rng ^= m_rng >> 17; m_rng ^= m_rng << 5;
    return (m_rng & 0xFFFFFF) / 16777216.0f;
}

float GameAudio::distanceToListener(const glm::vec3& p) const
{
    glm::vec3 d = p - m_listener;
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// ---------------------------------------------------------------- ambience loops
void GameAudio::startAmbience()
{
    const Sound s[3] = { Sound::AmbIndoor, Sound::AmbWind, Sound::AmbSiren };
    for (int i = 0; i < 3; ++i)
        if (m_amb[i] < 0 || !m_a.isPlaying(m_amb[i])) { m_amb[i] = m_a.playLoop(s[i], 0.0f, 0.2f); m_ambLast[i] = 0.0f; }
}

void GameAudio::stopAmbience(float fade)
{
    for (int i = 0; i < 3; ++i) { if (m_amb[i] >= 0) m_a.stop(m_amb[i], fade); m_amb[i] = -1; m_ambLast[i] = -1.0f; }
}

void GameAudio::setAmbience(float indoor, float wind, float siren, float fade)
{
    const float v[3] = { indoor, wind, siren };
    for (int i = 0; i < 3; ++i)
        if (m_amb[i] >= 0 && std::fabs(v[i] - m_ambLast[i]) > 0.01f) { m_a.setVoiceVolume(m_amb[i], v[i], fade); m_ambLast[i] = v[i]; }
}

// ---------------------------------------------------------------- game state changes
void GameAudio::stateChanged(GameState from, GameState to)
{
    if (to == GameState::Paused) { m_a.setPaused(true); return; }
    if (from == GameState::Paused) m_a.setPaused(false);
    if (from == GameState::Paused && to == GameState::Playing) return;           // just continue where we were

    switch (to) {
    case GameState::MainMenu:
        stopAmbience(1.0f);
        m_a.stopBus(Bus::Sfx, 0.3f);
        if (m_music >= 0) m_a.stop(m_music, 0.5f);
        m_music = m_a.playLoop(Sound::MenuMusic, 1.0f, 2.5f);                   // dark, slow menu music
        break;
    case GameState::Intro:
        if (m_music >= 0) { m_a.stop(m_music, 2.0f); m_music = -1; }
        m_broadcastDone = false;
        startAmbience();                                                        // sirens over a quiet room
        break;
    case GameState::Playing:
        if (m_music >= 0) { m_a.stop(m_music, 1.0f); m_music = -1; }
        m_a.stopBus(Bus::Music, 1.0f);
        m_a.stopBus(Bus::Sfx, 0.3f);                                            // restart: cut the old fight sounds
        startAmbience();
        break;
    case GameState::MissionFailed:
        setAmbience(0.1f, 0.1f, 0.12f, 2.0f);
        m_a.stopBus(Bus::Sfx, 0.4f);
        m_a.play(Sound::MissionFailed, 1.0f);                                   // a dark, sinking drone
        break;
    case GameState::MissionCompleted:
        setAmbience(0.0f, 0.18f, 0.08f, 3.0f);                                  // the world quiets down
        m_a.stopBus(Bus::Sfx, 0.6f);
        m_music = m_a.play(Sound::MissionCompleted, 1.0f);                      // gentle concluding music
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- every frame
void GameAudio::update(float dt, GameState state, const glm::vec3& listenerPos, const glm::vec3& listenerRight,
                       bool indoors, float introTime, const std::vector<FlickerProbe>& flicker)
{
    m_listener = listenerPos;
    m_right = listenerRight;
    m_a.setListener(listenerPos, listenerRight);
    m_groanCooldown -= dt;

    float want = indoors ? 1.0f : 0.0f;
    m_indoorBlend += (want - m_indoorBlend) * (1.0f - std::exp(-dt * 2.5f));     // walking through a door fades the ambience

    if (state == GameState::Intro) {
        setAmbience(0.35f, 0.0f, 0.95f, 1.5f);
        if (!m_broadcastDone && introTime >= 9.0f) { m_broadcastDone = true; m_a.play(Sound::EmergencyBroadcast, 0.9f); }
    } else if (state == GameState::Playing || state == GameState::Paused) {
        float b = m_indoorBlend;
        setAmbience(b, 1.0f - 0.88f * b, 0.8f - 0.5f * b, 0.5f);
    }

    // broken lights: a buzzing crackle every time one blinks near the player
    if (state == GameState::Playing) {
        if (m_prevFlicker.size() != flicker.size()) { m_prevFlicker.assign(flicker.size(), 1.0f); for (size_t i = 0; i < flicker.size(); ++i) m_prevFlicker[i] = flicker[i].intensity; }
        for (size_t i = 0; i < flicker.size(); ++i) {
            bool was = m_prevFlicker[i] > 0.5f, now = flicker[i].intensity > 0.5f;
            if (was != now) {
                float d = distanceToListener(flicker[i].pos);
                if (d < 13.0f) m_a.playAt(Sound::ElectricZap, flicker[i].pos, (now ? 1.0f : 0.7f) * (1.0f - d / 14.0f), between(0.9f, 1.15f));
            }
            m_prevFlicker[i] = flicker[i].intensity;
        }
    }
}

// ---------------------------------------------------------------- player sounds
void GameAudio::footstep(bool run, bool indoors)
{
    Sound s = run ? (indoors ? Sound::StepRunIndoor : Sound::StepRunOutdoor) : (indoors ? Sound::StepWalkIndoor : Sound::StepWalkOutdoor);
    m_leftFoot = !m_leftFoot;
    m_a.play(s, between(0.8f, 1.0f), between(0.93f, 1.07f), m_leftFoot ? -0.15f : 0.15f);
}
void GameAudio::jump()        { m_a.play(Sound::Jump, between(0.8f, 1.0f), between(0.95f, 1.05f)); }
void GameAudio::land()        { m_a.play(Sound::Land, between(0.85f, 1.0f), between(0.95f, 1.05f)); }
void GameAudio::pistolShot()  { m_a.play(Sound::PistolShot, between(0.92f, 1.0f), between(0.96f, 1.04f)); }
void GameAudio::pistolEmpty() { m_a.play(Sound::PistolEmpty, 0.9f, between(0.97f, 1.03f)); }
void GameAudio::pistolReload(){ m_a.play(Sound::PistolReload, 0.9f); }
void GameAudio::playerHurt()  { m_a.play(Sound::PlayerHurt, between(0.9f, 1.0f), between(0.95f, 1.05f)); }
void GameAudio::playerDeath() { m_a.play(Sound::PlayerDeath, 1.0f); }

void GameAudio::bulletImpact(const glm::vec3& at)
{
    float d = distanceToListener(at);
    if (d < 40.0f) m_a.playAt(Sound::BulletImpact, at, between(0.7f, 1.0f), between(0.9f, 1.15f));
}

// ---------------------------------------------------------------- zombies (3D: louder when near, panned, muffled behind walls)
void GameAudio::zombieEvent(ZombieEvent e, const glm::vec3& at)
{
    float d = distanceToListener(at);
    glm::vec3 ear = m_listener + glm::vec3(0.0f, 0.0f, 0.0f);
    bool muffled = (isBlocked && d > 1.5f) ? isBlocked(ear, at + glm::vec3(0.0f, 1.2f, 0.0f)) : false;

    switch (e) {
    case ZombieEvent::Groan:
        // 12 zombies groan on their own timers: allow one every second or so, never in a continuous wall of noise
        if (d > 34.0f || m_groanCooldown > 0.0f) return;
        m_groanCooldown = between(1.2f, 2.8f);
        m_a.playAt(Sound::ZombieGroan, at, between(0.65f, 1.0f), between(0.86f, 1.12f), muffled);
        break;
    case ZombieEvent::Detect:
        if (d < 32.0f) m_a.playAt(Sound::ZombieAlert, at, between(0.85f, 1.0f), between(0.92f, 1.1f), muffled);
        break;
    case ZombieEvent::Attack:
        if (d < 22.0f) m_a.playAt(Sound::ZombieAttack, at, between(0.85f, 1.0f), between(0.92f, 1.08f), muffled);
        break;
    case ZombieEvent::Hit:
        if (d < 40.0f) m_a.playAt(Sound::ZombieHit, at, between(0.85f, 1.0f), between(0.9f, 1.12f), muffled);
        break;
    case ZombieEvent::Death:
        if (d < 40.0f) m_a.playAt(Sound::ZombieDeath, at, between(0.9f, 1.0f), between(0.92f, 1.06f), muffled);
        break;
    case ZombieEvent::Step:
        if (d < 14.0f) m_a.playAt(Sound::ZombieStep, at, between(0.4f, 0.6f), between(0.85f, 1.05f), muffled);
        break;
    case ZombieEvent::RunStep:
        if (d < 18.0f) m_a.playAt(Sound::ZombieStep, at, between(0.65f, 0.9f), between(1.05f, 1.25f), muffled);
        break;
    }
}

// ---------------------------------------------------------------- mission and interface
void GameAudio::missionEvent(MissionEvent e)
{
    switch (e) {
    case MissionEvent::CardCollected:     m_a.play(Sound::KeycardCollected, 1.0f); break;
    case MissionEvent::AmmoCollected:     m_a.play(Sound::AmmoPickup, 1.0f); break;
    case MissionEvent::PanelDenied:       m_a.play(Sound::BoothDenied, 1.0f); break;
    case MissionEvent::GateLocked:        m_a.play(Sound::BoothDenied, 0.7f, 0.8f); break;
    case MissionEvent::GateUnlocked:
        m_a.play(Sound::BoothBeep, 1.0f);
        m_a.play(Sound::GateUnlocked, 1.0f);
        m_a.playAt(Sound::GateOpen, sites::kGate, 1.0f);                     // the motor and chain of the sliding gate
        break;
    case MissionEvent::ObjectiveComplete: m_a.play(Sound::ObjectiveComplete, 0.9f); break;
    case MissionEvent::Completed:         break;                              // the music starts with the state change
    case MissionEvent::Failed:            break;                              // so does the failure sound
    }
}

void GameAudio::uiSelect()  { m_a.play(Sound::UiSelect, 1.0f); }
void GameAudio::uiConfirm() { m_a.play(Sound::UiConfirm, 1.0f); }
