// src/game/Settings.h
// The player's options: four volumes and the mouse sensitivity. They are saved to a small text file
// (settings.cfg, next to the game) and loaded again at the next start.
//   master=0.80
//   music=0.60
//   ...
#pragma once
#include <string>

struct Settings {
    float masterVolume = 0.80f;      // 0..1
    float musicVolume = 0.60f;
    float sfxVolume = 0.90f;
    float ambienceVolume = 0.70f;
    float mouseSensitivity = 0.10f;  // degrees per pixel (the Camera's own unit)

    static constexpr float kMinSensitivity = 0.02f;
    static constexpr float kMaxSensitivity = 0.40f;

    void clampValues();                                  // keep everything in its legal range
    bool load(const std::string& path);                  // false if the file does not exist (defaults stay)
    bool save(const std::string& path) const;
};
