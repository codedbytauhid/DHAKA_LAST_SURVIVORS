// src/game/Mission.cpp
#include "game/Mission.h"
#include "world/Sites.h"
#include <algorithm>
#include <cmath>

using glm::vec3;

static float horizDist(const vec3& a, const vec3& b)
{
    float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

MissionManager::MissionManager() { resetMissionData(); }

// ------------------------------------------------------------------ flow
void MissionManager::resetMissionData()
{
    m_stage = 0;
    for (int i = 0; i < StageCount; ++i) m_done[i] = false;
    m_hasCard = m_gateUnlocked = m_checkpointActive = false;
    for (int i = 0; i < kAmmoSites; ++i) m_ammoTaken[i] = false;
    m_pendingAmmo = 0;
    m_gateProgress = 0.0f;
    m_introT = m_playT = m_failTimer = 0.0f;
    m_killed = 0;
    m_finalHealth = 0;
    m_prompt.clear();
    m_queue.clear();
    m_message.clear();
    m_hasMessage = false;
    m_msgT = m_msgLen = 0.0f;
}

void MissionManager::beginPlaying()
{
    m_state = GameState::Playing;
    pushMessage("SOMETHING IS WRONG...", 2.6f);
    pushMessage("THE OUTBREAK HAS REACHED THE CAMPUS.", 3.2f);
    pushMessage("FIND A WAY OUT.", 2.6f);
}

void MissionManager::startMission()
{
    resetMissionData();
    m_state = GameState::Intro;
}

void MissionManager::skipIntro()
{
    if (m_state == GameState::Intro) beginPlaying();
}

void MissionManager::restart()
{
    resetMissionData();
    beginPlaying();
}

void MissionManager::pause()  { if (m_state == GameState::Playing) m_state = GameState::Paused; }
void MissionManager::resume() { if (m_state == GameState::Paused)  m_state = GameState::Playing; }
void MissionManager::goToMainMenu() { resetMissionData(); m_state = GameState::MainMenu; }

// ------------------------------------------------------------------ messages
void MissionManager::pushMessage(const std::string& text, float seconds)
{
    // Do not repeat a line that is already showing or waiting (pressing E again and again must not spam the screen).
    if (m_hasMessage && m_message == text) return;
    for (const Msg& q : m_queue) if (q.text == text) return;
    if (m_queue.size() >= 4) m_queue.pop_front();
    m_queue.push_back({ text, seconds });
}

float MissionManager::messageAlpha() const
{
    if (!m_hasMessage) return 0.0f;
    float fadeIn = std::min(1.0f, m_msgT / 0.4f);
    float fadeOut = std::min(1.0f, (m_msgLen - m_msgT) / 0.6f);
    return std::max(0.0f, std::min(fadeIn, fadeOut));
}

float MissionManager::flickerOverlay() const
{
    // The lights go out and flicker back at the start of the mission: black frames at hashed moments, fading away.
    const float len = 2.0f;
    if (m_state != GameState::Playing || m_playT >= len) return 0.0f;
    if (m_playT < 0.35f) return 1.0f - m_playT / 0.35f * 0.15f;           // starts almost fully dark
    float n = std::floor(m_playT * 14.0f);
    float v = std::sin(n * 12.9898f) * 43758.5453f; v -= std::floor(v);
    return v < 0.45f ? 0.85f * (1.0f - m_playT / len) : 0.0f;
}

// ------------------------------------------------------------------ objectives
const char* MissionManager::objectiveText() const
{
    switch (m_stage) {
    case StageEscapeClassroom: return "FIND A WAY OUT OF THE CLASSROOM.";
    case StageFindCard:        return "FIND THE ACCESS CARD.";
    case StageReachCourtyard:  return "REACH THE COURTYARD.";
    case StageUnlockGate:      return "UNLOCK THE UNIVERSITY GATE.";
    case StageEscapeGate:      return "ESCAPE THROUGH THE UNIVERSITY GATE.";
    default:                   return "REACH THE SAFE CHECKPOINT.";
    }
}

const char* MissionManager::objectiveHint() const
{
    switch (m_stage) {
    case StageEscapeClassroom: return "THE DOOR IS AT THE BACK OF THE ROOM.";
    case StageFindCard:        return "SECURITY KEEP THEIR CARDS UPSTAIRS. USE THE STAIRS AT THE END OF THE CORRIDOR.";
    case StageReachCourtyard:  return "TAKE THE EXIT DOOR IN THE MIDDLE OF THE CORRIDOR.";
    case StageUnlockGate:      return "USE THE CARD AT THE PANEL IN THE SECURITY BOOTH NEXT TO THE GATE.";
    case StageEscapeGate:      return "THE GATE IS OPEN. RUN!";
    default:                   return "FOLLOW THE GREEN LIGHT ON THE ROAD.";
    }
}

vec3 MissionManager::markerTarget() const
{
    switch (m_stage) {
    case StageEscapeClassroom: return sites::kClassroomDoor;
    case StageFindCard:        return sites::kCard;
    case StageReachCourtyard:  return sites::kCourtyard;
    case StageUnlockGate:      return sites::kPanel;
    case StageEscapeGate:      return sites::kBeyondGate;
    default:                   return sites::kCheckpoint + vec3(0.0f, 1.5f, 0.0f);
    }
}

int MissionManager::objectivesDone() const
{
    int n = 0;
    for (int i = 0; i < StageCount; ++i) if (m_done[i]) ++n;
    return n;
}

bool MissionManager::stageComplete(int s, const MissionFrame& f) const
{
    const vec3& p = f.playerPos;
    switch (s) {
    case StageEscapeClassroom:      // standing in the corridor
        return p.x > -12.0f && p.x < 12.2f && p.z > 4.6f && p.z < 7.2f && p.y < 1.0f;
    case StageFindCard:     return m_hasCard;
    case StageReachCourtyard:       // out of the building, on the lawn / walkway side
        return m_hasCard && p.z > 9.5f && p.z < 34.0f && std::fabs(p.x) < 26.0f && p.y < 1.5f;
    case StageUnlockGate:   return m_gateUnlocked;
    case StageEscapeGate:   return m_gateUnlocked && p.z > 35.0f;
    case StageCheckpoint:   return m_checkpointActive && horizDist(p, sites::kCheckpoint) < sites::kCheckpointRadius;
    default:                return false;
    }
}

// ------------------------------------------------------------------ interactions
bool MissionManager::lineOfSight(const MissionFrame& f, const vec3& target, float margin) const
{
    if (!f.world) return true;
    vec3 o(f.playerPos.x, f.playerPos.y + 1.25f, f.playerPos.z);
    float dx = target.x - o.x, dy = target.y - o.y, dz = target.z - o.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist < 0.01f) return true;
    vec3 dir(dx / dist, dy / dist, dz / dist);
    return f.world->raycast(o, dir, dist) >= dist - margin;     // no wall between the player and the object
}

// Looks at everything the player could use right now and returns the NEAREST one that is in range, at a sensible
// height and not behind a wall. (Nearest, not "first in a list": the booth holds both the access panel and an
// ammunition crate, and E must act on the one you are actually standing next to.)
MissionManager::Target MissionManager::findInteractable(const MissionFrame& f, std::string& promptOut, int* indexOut) const
{
    const vec3& p = f.playerPos;
    promptOut.clear();

    Target best = Target::None;
    float bestDist = 1.0e9f;
    int bestIndex = -1;
    const char* bestPrompt = "";

    auto consider = [&](Target t, const vec3& at, float radius, float losMargin, const char* prompt, int index) {
        float d = horizDist(p, at);
        if (d >= radius || d >= bestDist) return;
        if (std::fabs((p.y + 1.0f) - at.y) >= 1.6f) return;       // on another floor
        if (!lineOfSight(f, at, losMargin)) return;                // behind a wall
        best = t; bestDist = d; bestIndex = index; bestPrompt = prompt;
    };

    if (m_stage == StageFindCard && !m_hasCard)
        consider(Target::Card, sites::kCard, 2.0f, 0.35f, "PRESS E TO COLLECT ACCESS CARD", -1);
    for (int i = 0; i < kAmmoSites; ++i)
        if (!m_ammoTaken[i]) consider(Target::Ammo, sites::kAmmo[i], 1.8f, 0.4f, "PRESS E TO COLLECT AMMUNITION", i);
    if (!m_gateUnlocked) {
        consider(Target::Panel, sites::kPanel, 1.9f, 0.3f, m_hasCard ? "PRESS E TO USE ACCESS CARD" : "PRESS E TO USE THE ACCESS PANEL", -1);
        consider(Target::Gate, sites::kGate, 2.3f, 0.3f, "PRESS E TO TRY THE GATE", -1);
    }

    if (best != Target::None) promptOut = bestPrompt;
    if (indexOut) *indexOut = bestIndex;
    return best;
}

void MissionManager::interact(const MissionFrame& f)
{
    if (m_state != GameState::Playing || !f.playerAlive) return;
    std::string prompt;
    int index = -1;
    switch (findInteractable(f, prompt, &index)) {
    case Target::Card:
        m_hasCard = true;                                   // the prompt disappears with the card: it cannot be taken twice
        pushMessage("ACCESS CARD ACQUIRED.", 2.6f);
        break;
    case Target::Ammo:
        m_ammoTaken[index] = true;                          // one-time pickup; it also vanishes from the level
        m_pendingAmmo += kAmmoPerPickup;
        pushMessage("AMMUNITION +12.", 2.0f);
        break;
    case Target::Panel:
        if (m_hasCard) {
            m_gateUnlocked = true;
            m_checkpointActive = true;
            pushMessage("GATE ACCESS GRANTED.", 2.4f);
            pushMessage("THE EXIT IS OPEN. RUN!", 3.0f);
        } else {
            pushMessage("ACCESS DENIED. YOU NEED AN ACCESS CARD.", 2.8f);
        }
        break;
    case Target::Gate:
        pushMessage("THE GATE IS LOCKED. THE CONTROL PANEL IS IN THE SECURITY BOOTH.", 3.4f);
        break;
    case Target::None:
        break;
    }
}

void MissionManager::debugGiveCard(const MissionFrame&)
{
    if (m_state == GameState::Playing && !m_hasCard) {
        m_hasCard = true;
        pushMessage("ACCESS CARD ACQUIRED. (DEBUG)", 2.0f);
    }
}

// ------------------------------------------------------------------ per-frame update
void MissionManager::update(const MissionFrame& f)
{
    if (m_state == GameState::Intro) {
        m_introT += f.dt;
        if (m_introT >= kIntroLength) beginPlaying();
        return;
    }
    if (m_state != GameState::Playing) return;

    m_playT += f.dt;

    // message queue: show one line at a time
    if (m_hasMessage) {
        m_msgT += f.dt;
        if (m_msgT >= m_msgLen) m_hasMessage = false;
    }
    if (!m_hasMessage && !m_queue.empty()) {
        m_message = m_queue.front().text;
        m_msgLen = m_queue.front().seconds;
        m_msgT = 0.0f;
        m_hasMessage = true;
        m_queue.pop_front();
    }

    // the gate slides open for 2.5 seconds after it was unlocked
    if (m_gateUnlocked && m_gateProgress < 1.0f) m_gateProgress = std::min(1.0f, m_gateProgress + f.dt / 2.5f);

    // failure: a short pause so the player can see what happened, then the MISSION FAILED screen
    if (!f.playerAlive) {
        m_prompt.clear();
        m_failTimer += f.dt;
        if (m_failTimer >= 1.6f) {
            m_finalHealth = 0;
            m_killed = f.zombiesTotal - f.zombiesAlive;
            m_state = GameState::MissionFailed;
        }
        return;
    }

    // objectives: only the CURRENT stage can be completed, so they always happen in order
    while (m_stage < StageCount && stageComplete(m_stage, f)) {
        m_done[m_stage] = true;
        switch (m_stage) {
        case StageEscapeClassroom: pushMessage("STAY QUIET. FIND THE ACCESS CARD.", 3.0f); break;
        case StageFindCard:        break;                                 // "ACCESS CARD ACQUIRED." was already shown
        case StageReachCourtyard:  pushMessage("FIND THE SECURITY BOOTH BY THE GATE.", 3.0f); break;
        case StageUnlockGate:      break;                                 // gate messages were already shown
        case StageEscapeGate:      pushMessage("THE CHECKPOINT IS JUST AHEAD.", 2.6f); break;
        case StageCheckpoint:
            pushMessage("YOU MADE IT. YOU'RE SAFE.", 3.0f);
            m_finalHealth = (int)std::ceil(f.playerHealth);
            m_killed = f.zombiesTotal - f.zombiesAlive;
            m_prompt.clear();
            m_state = GameState::MissionCompleted;
            return;
        }
        ++m_stage;
    }

    findInteractable(f, m_prompt);
}
