// src/game/Settings.cpp
#include "game/Settings.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

static float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

void Settings::clampValues()
{
    masterVolume = clamp01(masterVolume);
    musicVolume = clamp01(musicVolume);
    sfxVolume = clamp01(sfxVolume);
    ambienceVolume = clamp01(ambienceVolume);
    mouseSensitivity = std::max(kMinSensitivity, std::min(kMaxSensitivity, mouseSensitivity));
}

bool Settings::load(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        char* end = nullptr;
        float value = std::strtof(line.c_str() + eq + 1, &end);
        if (end == line.c_str() + eq + 1) continue;                 // not a number: ignore the line
        if (key == "master") masterVolume = value;
        else if (key == "music") musicVolume = value;
        else if (key == "sfx") sfxVolume = value;
        else if (key == "ambience") ambienceVolume = value;
        else if (key == "sensitivity") mouseSensitivity = value;
    }
    clampValues();                                                  // a hand-edited file can never break the game
    return true;
}

bool Settings::save(const std::string& path) const
{
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << "master=" << masterVolume << "\n"
        << "music=" << musicVolume << "\n"
        << "sfx=" << sfxVolume << "\n"
        << "ambience=" << ambienceVolume << "\n"
        << "sensitivity=" << mouseSensitivity << "\n";
    return out.good();
}
