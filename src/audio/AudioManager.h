// src/audio/AudioManager.h
// A small software audio mixer.
//
//   * Every sound is a mono array of floats (see Sounds.h). A "voice" is one sound that is currently playing.
//   * mix() adds all voices together into a stereo buffer: volume, stereo pan, distance fade, pitch (playback
//     speed) and an optional "muffled" low-pass filter are applied per voice.
//   * On Windows a background thread keeps a few small buffers queued to the sound card with the built-in
//     WinMM API (waveOut). Link with -lwinmm. On other systems (or if no sound device exists) the game simply
//     runs silently; mix() can still be called by tests.
//
// Volume controls: master, music, sfx (sound effects), ambience.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "audio/Sounds.h"

class AudioManager {
public:
    struct Backend;            // the Windows sound-card part: defined only in AudioManager.cpp
    struct Lock;               // tiny mutex (Windows CRITICAL_SECTION; nothing on other systems)

    AudioManager();
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // Creates every sound (and loads WAV overrides from wavDir). openDevice=false: no sound card (tests / export).
    bool init(bool openDevice, const std::string& wavDir);
    void shutdown();
    bool deviceOpen() const;

    // ---- settings
    void setVolumes(float master, float music, float sfx, float ambience);       // 0..1 each, applied immediately
    void setListener(const glm::vec3& position, const glm::vec3& rightVector);   // the "ears": for 3D sounds
    void setPaused(bool paused);   // freezes sound effects and ambience, ducks the music; interface sounds still play

    // ---- playing. All return a handle (or -1 if nothing was started).
    int play(Sound s, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f);                    // plain stereo sound
    int playAt(Sound s, const glm::vec3& worldPos, float volume = 1.0f, float pitch = 1.0f, bool muffled = false);  // 3D sound
    int playLoop(Sound s, float volume, float fadeInSeconds);                                        // loops until stopped
    void setVoiceVolume(int handle, float volume, float fadeSeconds);                                // smooth volume change
    void stop(int handle, float fadeSeconds);
    void stopBus(Bus bus, float fadeSeconds);
    void stopAll(float fadeSeconds);
    bool isPlaying(int handle) const;
    int activeVoices() const;

    // ---- the mixer itself (called by the audio thread, or directly by tests)
    void mix(float* interleavedStereo, int frames);

    // ---- files
    int exportWavs(const std::string& dir) const;       // writes the synthesised sounds as WAV files; returns how many
    static bool writeWav(const std::string& path, const std::vector<float>& mono, int sampleRate);
    static bool loadWav(const std::string& path, std::vector<float>& monoOut);   // 8/16/24/32-bit PCM or float, any rate -> 44.1 kHz mono

    const std::vector<float>& samples(Sound s, int variant) const { return m_bank[(int)s][variant]; }
    int variantCount(Sound s) const { return (int)m_bank[(int)s].size(); }

private:
    struct Voice {
        bool active = false, loop = false, positional = false, muffled = false, stopping = false, ignorePause = false;
        int handle = 0;
        unsigned serial = 0;
        Sound sound = Sound::UiSelect;
        Bus bus = Bus::Sfx;
        const std::vector<float>* data = nullptr;
        double pos = 0.0;
        float pitch = 1.0f, gain = 0.0f, target = 1.0f, smooth = 1.0f, pan = 0.0f, lp = 0.0f;
        glm::vec3 worldPos{0.0f};
    };

    int startVoice(Sound s, float volume, float pitch, bool loop, float fadeInSeconds);
    Voice* find(int handle);
    const Voice* find(int handle) const;
    float fadeToSmooth(float seconds) const;
    unsigned nextRandom();

    std::vector<std::vector<float>> m_bank[(int)Sound::Count];
    int m_lastVariant[(int)Sound::Count];
    std::vector<Voice> m_voices;
    float m_busTarget[3] = { 1, 1, 1 }, m_busGain[3] = { 1, 1, 1 };
    float m_masterTarget = 1.0f, m_masterGain = 1.0f;
    bool m_paused = false;
    glm::vec3 m_listenerPos{0.0f}, m_listenerRight{1.0f, 0.0f, 0.0f};
    int m_nextHandle = 1;
    unsigned m_serial = 0, m_rng = 2463534242u;
    bool m_ready = false;

    Backend* m_backend = nullptr;
    Lock* m_lock = nullptr;
    friend struct BackendAccess;
};
