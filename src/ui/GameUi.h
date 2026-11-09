// src/ui/GameUi.h
// All the screens of the game, drawn with the Hud toolkit: main menu, controls, settings, intro, in-game HUD,
// pause menu, mission failed and mission completed. It only DRAWS; main.cpp decides which screen is active.
#pragma once
#include <string>
#include <vector>
#include "ui/Hud.h"
#include "game/Mission.h"
#include "game/Settings.h"

struct MarkerInfo {                // where the objective marker goes on screen
    bool valid = false;
    float x = 0.0f, y = 0.0f;      // pixels
    float distance = 0.0f;         // metres
    bool offscreen = false;
};

struct PlayingInfo {
    float healthFraction = 1.0f;
    int healthPercent = 100;
    int zombiesAlive = 0;
    float hurtFlash = 0.0f;        // 1 right after a hit, fades to 0
    bool debugMode = false;
    bool godMode = false;
    MarkerInfo marker;
    double time = 0.0;             // seconds, for pulsing effects

    // combat
    int ammoMagazine = 12;
    int ammoCapacity = 12;
    int ammoReserve = 36;
    bool reloading = false;
    float reloadProgress = 0.0f;   // 0..1
    float aimBlend = 0.0f;         // 0..1, changes the crosshair
    int kills = 0;
    float hitMarker = 0.0f;        // 1 right after the bullet hit a zombie, fades to 0
    bool hitKilled = false;        // that hit was a kill (the marker turns red)
    bool alive = true;

    // short banner such as "OBJECTIVE COMPLETE" (alpha 0 = hidden)
    std::string toast;
    float toastAlpha = 0.0f;
};

class GameUi {
public:
    explicit GameUi(const Hud& hud) : m_hud(hud) {}

    void mainMenu(int selected, float t) const;
    void controls() const;
    void settings(int row, const Settings& s) const;
    void intro(const MissionManager& m) const;
    void playing(const MissionManager& m, const PlayingInfo& info) const;
    void paused(int selected) const;
    void failed(float t) const;                 // t = seconds since the screen appeared (fade-in)
    void completed(const MissionManager& m, float t) const;

    static const std::vector<std::string>& mainMenuItems();
    static const std::vector<std::string>& pauseMenuItems();
    static constexpr int kSettingsRows = 5;

private:
    float ui() const { return (float)m_hud.height() / 720.0f; }     // 1.0 at 720p: everything scales with the window
    void menuList(const std::vector<std::string>& items, int selected, float y, float px) const;
    void vignette(float strength) const;                            // darkened screen edges
    const Hud& m_hud;
};
