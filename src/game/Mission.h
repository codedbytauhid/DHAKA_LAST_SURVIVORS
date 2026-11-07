// src/game/Mission.h
// THE FALL OF BARIDHARA: game states, objectives, interactions, messages and statistics.
// This class contains NO drawing code. main.cpp feeds it the player's position and health each frame,
// then reads its answers (which objective is active, is there a prompt, is the gate open ...) to draw the
// HUD and to update the level.
//
//  STATES                                                         OBJECTIVE STAGES (done in this order)
//   MainMenu -> Intro -> Playing <-> Paused                         0 escape the classroom (enter the corridor)
//                          |                                        1 find the access card (press E)
//                          +-> MissionFailed  (player died)         2 reach the courtyard
//                          +-> MissionCompleted (reached checkpoint) 3 unlock the gate (card + booth panel, E)
//   restart(): any state -> Playing, everything reset              4 escape through the gate
//                                                                   5 reach the safe checkpoint
#pragma once
#include <deque>
#include <string>
#include <glm/glm.hpp>
#include "world/Collision.h"

enum class GameState { MainMenu, Intro, Playing, Paused, MissionFailed, MissionCompleted };

enum MissionStage {
    StageEscapeClassroom = 0, StageFindCard, StageReachCourtyard, StageUnlockGate, StageEscapeGate, StageCheckpoint,
    StageCount
};

// What the mission needs to know about the world this frame.
struct MissionFrame {
    float dt = 0.0f;
    glm::vec3 playerPos{0.0f};      // feet
    bool playerAlive = true;
    int zombiesAlive = 0;
    int zombiesTotal = 0;
    float playerHealth = 100.0f;
    const CollisionWorld* world = nullptr;   // used to stop interactions through walls
};

class MissionManager {
public:
    static constexpr float kIntroLength = 16.0f;
    static constexpr int kAmmoSites = 4;          // ammunition crates in the level
    static constexpr int kAmmoPerPickup = 12;     // bullets added to the reserve by one crate

    MissionManager();

    // ---- flow
    GameState state() const { return m_state; }
    void startMission();      // MainMenu -> Intro (everything reset)
    void skipIntro();         // Intro -> Playing
    void restart();           // anything -> Playing, everything reset (used after failing or from the pause menu)
    void pause();             // Playing -> Paused
    void resume();            // Paused -> Playing
    void goToMainMenu();      // anything -> MainMenu

    // ---- per frame
    void update(const MissionFrame& f);     // timers, objective checks, fail / complete
    void interact(const MissionFrame& f);   // the player pressed E
    void debugGiveCard(const MissionFrame& f);   // debug mode only (key U): collects the card as if E was pressed

    // ---- objectives
    int stage() const { return m_stage; }
    bool stageDone(int s) const { return s >= 0 && s < StageCount && m_done[s]; }
    int objectivesDone() const;
    const char* objectiveText() const;
    const char* objectiveHint() const;
    glm::vec3 markerTarget() const;          // where the on-screen marker should point

    // ---- interactions
    bool hasPrompt() const { return !m_prompt.empty(); }
    const std::string& prompt() const { return m_prompt; }

    // ---- ammunition crates (each can be taken once per mission)
    bool ammoTaken(int i) const { return i >= 0 && i < kAmmoSites && m_ammoTaken[i]; }
    int takeAmmoReward() { int r = m_pendingAmmo; m_pendingAmmo = 0; return r; }   // main adds this many bullets to the weapon

    // ---- mission props
    bool hasCard() const { return m_hasCard; }
    bool gateUnlocked() const { return m_gateUnlocked; }
    float gateProgress() const { return m_gateProgress; }     // 0 closed .. 1 open
    bool checkpointActive() const { return m_checkpointActive; }

    // ---- messages (short lines that fade in and out)
    bool hasMessage() const { return m_hasMessage; }
    const std::string& messageText() const { return m_message; }
    float messageAlpha() const;
    void pushMessage(const std::string& text, float seconds);

    // ---- other
    float introTime() const { return m_introT; }
    float playTime() const { return m_playT; }           // seconds of actual play (pauses excluded)
    float flickerOverlay() const;                        // 0..1 black overlay: the power flicker at the start
    int zombiesKilled() const { return m_killed; }
    int finalHealthPercent() const { return m_finalHealth; }

private:
    enum class Target { None, Card, Ammo, Panel, Gate };
    Target findInteractable(const MissionFrame& f, std::string& promptOut, int* indexOut = nullptr) const;
    bool lineOfSight(const MissionFrame& f, const glm::vec3& target, float margin) const;
    void resetMissionData();
    void beginPlaying();
    bool stageComplete(int s, const MissionFrame& f) const;

    struct Msg { std::string text; float seconds; };

    GameState m_state = GameState::MainMenu;
    int m_stage = 0;
    bool m_done[StageCount] = {};
    bool m_hasCard = false, m_gateUnlocked = false, m_checkpointActive = false;
    bool m_ammoTaken[kAmmoSites] = {};
    int m_pendingAmmo = 0;
    float m_gateProgress = 0.0f;
    float m_introT = 0.0f, m_playT = 0.0f, m_failTimer = 0.0f;
    int m_killed = 0, m_finalHealth = 0;
    std::string m_prompt;

    std::deque<Msg> m_queue;
    std::string m_message;
    bool m_hasMessage = false;
    float m_msgT = 0.0f, m_msgLen = 0.0f;
};
