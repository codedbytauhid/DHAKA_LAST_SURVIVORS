// src/audio/Sounds.h
// The list of every sound in the game, how each one behaves, and the code that creates it.
//
// There are NO audio files to download: every sound is synthesised from noise, sine waves and filters when the
// game starts (see SoundBank.cpp). If you drop a WAV file called  assets/audio/<name>.wav  next to the game, it
// replaces the synthesised sound with that name (for variations use <name>_1.wav, <name>_2.wav ...).
#pragma once
#include <vector>

constexpr int kSampleRate = 44100;           // every sound is mono, 32-bit float, at this rate

enum class Bus { Music, Sfx, Ambience };

enum class Sound {
    // player
    StepWalkIndoor, StepWalkOutdoor, StepRunIndoor, StepRunOutdoor, Jump, Land,
    PistolShot, PistolReload, PistolEmpty, BulletImpact, PlayerHurt, PlayerDeath,
    // zombies
    ZombieGroan, ZombieAlert, ZombieStep, ZombieAttack, ZombieHit, ZombieDeath,
    // environment (loops) and one-shots
    AmbIndoor, AmbWind, AmbSiren, ElectricZap, GateOpen, BoothBeep, BoothDenied, EmergencyBroadcast,
    // interface and mission
    UiSelect, UiConfirm, ObjectiveComplete, KeycardCollected, AmmoPickup, GateUnlocked, MissionFailed,
    // music
    MenuMusic, MissionCompleted,
    Count
};

struct SoundInfo {
    const char* name;       // also the WAV file name
    Bus bus;
    bool loop;
    float gain;             // base loudness of this sound (the mixer balance lives here)
    int maxVoices;          // at most this many copies of it play at once (the oldest is faded out)
    bool ignorePause;       // interface sounds keep playing while the game is paused
};

const SoundInfo& soundInfo(Sound s);

// Builds the sound (several slightly different variations for sounds that repeat a lot).
std::vector<std::vector<float>> synthesize(Sound s);
