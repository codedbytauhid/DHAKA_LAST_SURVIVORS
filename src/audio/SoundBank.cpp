// src/audio/SoundBank.cpp
// Sound synthesis. Everything here is plain arithmetic on arrays of floats, so it works (and is tested) without
// a sound card. The basic ingredients:
//   noise  - random numbers: wind, breath, footsteps, gunshot "crack"
//   sine / saw waves - tones, voices, sirens, hums
//   filters - low-pass (removes high pitch = muffled), high-pass, band-pass ("formants" shape a voice: they make
//             a buzzing saw wave sound like "ooh" or "aah")
//   envelopes - a volume curve over time (sudden start, slow fade...)
//   reverb  - a few echoes, makes the gunshot sound like it happens in a big space
#include "audio/Sounds.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

const float PI = 3.14159265f;
const float SR = (float)kSampleRate;

// ---------------------------------------------------------------- tools
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed * 2654435761u + 12345u) {}
    uint32_t u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (u() & 0xFFFFFF) / 16777216.0f; }      // 0..1
    float n() { return f() * 2.0f - 1.0f; }                    // -1..1 (white noise)
};

struct LowPass {
    float a = 0.0f, y = 0.0f;
    void setCutoff(float hz) { a = 1.0f - std::exp(-2.0f * PI * hz / SR); }
    float process(float x) { y += a * (x - y); return y; }
};
struct HighPass {
    LowPass lp;
    void setCutoff(float hz) { lp.setCutoff(hz); }
    float process(float x) { return x - lp.process(x); }
};
struct BandPass {                                     // resonant band-pass (RBJ biquad)
    float b0 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void set(float hz, float q)
    {
        float w = 2.0f * PI * std::min(hz, SR * 0.45f) / SR, al = std::sin(w) / (2.0f * q), a0 = 1.0f + al;
        b0 = al / a0; b2 = -al / a0; a1 = -2.0f * std::cos(w) / a0; a2 = (1.0f - al) / a0;
    }
    float process(float x)
    {
        float y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

typedef std::vector<float> Buf;

Buf make(float seconds) { return Buf((size_t)(seconds * SR), 0.0f); }
float lerp(float a, float b, float t) { return a + (b - a) * t; }
float expEnv(float t, float tau) { return std::exp(-t / tau); }

void add(Buf& dst, const Buf& src, float startSeconds, float gain)
{
    size_t off = (size_t)(startSeconds * SR);
    if (dst.size() < off + src.size()) dst.resize(off + src.size(), 0.0f);
    for (size_t i = 0; i < src.size(); ++i) dst[off + i] += src[i] * gain;
}

void normalize(Buf& v, float peak)
{
    float m = 0.0f;
    for (float x : v) m = std::max(m, std::fabs(x));
    if (m < 1.0e-6f) return;
    float g = peak / m;
    for (float& x : v) x *= g;
}

void fadeEdges(Buf& v, float inSeconds, float outSeconds)       // avoids clicks at the start / end
{
    size_t a = (size_t)(inSeconds * SR), b = (size_t)(outSeconds * SR);
    for (size_t i = 0; i < a && i < v.size(); ++i) v[i] *= (float)i / (float)a;
    for (size_t i = 0; i < b && i < v.size(); ++i) v[v.size() - 1 - i] *= (float)i / (float)b;
}

Buf reverb(const Buf& in, float wet, float feedback, float tailSeconds)    // four echoing delay lines
{
    Buf out = in;
    out.resize(in.size() + (size_t)(tailSeconds * SR), 0.0f);
    const int delays[4] = { 1423, 1801, 2111, 2377 };
    for (int d : delays) {
        Buf line(d, 0.0f);
        size_t pos = 0;
        for (size_t i = 0; i < out.size(); ++i) {
            float x = (i < in.size()) ? in[i] : 0.0f;
            float y = line[pos];
            line[pos] = x + y * feedback;
            pos = (pos + 1) % line.size();
            out[i] += y * wet * 0.25f;
        }
    }
    return out;
}

// Makes a seamless loop: `v` holds length + fade samples; the extra tail is blended into the beginning.
Buf makeLoop(const Buf& v, float fadeSeconds)
{
    size_t f = (size_t)(fadeSeconds * SR);
    size_t n = v.size() - f;
    Buf out(v.begin(), v.begin() + n);
    for (size_t i = 0; i < f; ++i) {
        float w = (float)i / (float)f;
        out[i] = v[i] * std::sin(w * PI * 0.5f) + v[n + i] * std::cos(w * PI * 0.5f);   // equal-power crossfade
    }
    return out;
}

// ---------------------------------------------------------------- a voice-like sound (groans, grunts, screams)
struct VoiceParams {
    float len, f0a, f0b;               // length and pitch at the start / end (Hz)
    float vibHz, vibDepth;             // pitch wobble
    float f1a, f1b, f2a, f2b, q;       // two resonances ("formants") that move: they decide the vowel
    float attack, release;             // volume shape (seconds)
    float roughHz, roughDepth;         // rattling amplitude modulation: the "dead" growl
    float breath, drive;               // airy noise amount and distortion
};

Buf formantVoice(const VoiceParams& p, uint32_t seed)
{
    Rng rng(seed);
    Buf out = make(p.len);
    BandPass b1, b2, b3;
    b3.set(1500.0f, 1.5f);
    float phase = 0.0f;
    for (size_t i = 0; i < out.size(); ++i) {
        float t = (float)i / SR, u = t / p.len;
        if ((i & 15) == 0) {
            b1.set(lerp(p.f1a, p.f1b, u), p.q);
            b2.set(lerp(p.f2a, p.f2b, u), p.q);
        }
        float f0 = lerp(p.f0a, p.f0b, u) * (1.0f + p.vibDepth * std::sin(2.0f * PI * p.vibHz * t));
        phase += f0 / SR;
        phase -= std::floor(phase);
        float src = 2.0f * phase - 1.0f;                                           // saw wave: rich in harmonics
        src *= 1.0f + p.roughDepth * std::sin(2.0f * PI * p.roughHz * t);
        float v = b1.process(src) + 0.7f * b2.process(src) + p.breath * b3.process(rng.n());
        v = std::tanh(p.drive * v);
        float env = std::min(1.0f, t / std::max(0.001f, p.attack)) * std::min(1.0f, (p.len - t) / std::max(0.001f, p.release));
        out[i] = v * env;
    }
    return out;
}

// ---------------------------------------------------------------- small building blocks
Buf click(float freq, float tau, float len = 0.08f, uint32_t seed = 1)       // metallic click: a ping plus a noise tick
{
    Rng rng(seed);
    Buf b = make(len);
    HighPass hp; hp.setCutoff(2500.0f);
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        b[i] = (std::sin(2.0f * PI * freq * t) * 0.6f + hp.process(rng.n()) * 0.8f) * expEnv(t, tau);
    }
    return b;
}

Buf thump(float f0, float f1, float tau, float len)                          // a low drum-like "boom" that sinks in pitch
{
    Buf b = make(len);
    float phase = 0.0f;
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        phase += lerp(f0, f1, std::min(1.0f, t / (len * 0.6f))) / SR;
        b[i] = std::sin(2.0f * PI * phase) * expEnv(t, tau);
    }
    return b;
}

Buf noiseBurst(float len, float tau, float lowHz, float highHz, uint32_t seed)
{
    Rng rng(seed);
    Buf b = make(len);
    LowPass lp; lp.setCutoff(lowHz);
    HighPass hp; hp.setCutoff(std::max(5.0f, highHz));
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        b[i] = hp.process(lp.process(rng.n())) * expEnv(t, tau);
    }
    return b;
}

Buf bell(float freq, float decay, float len)
{
    Buf b = make(len);
    const float ratio[4] = { 1.0f, 2.76f, 5.4f, 8.93f }, amp[4] = { 1.0f, 0.35f, 0.15f, 0.06f }, dec[4] = { 1.0f, 0.6f, 0.35f, 0.2f };
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR, s = 0.0f;
        for (int k = 0; k < 4; ++k) s += amp[k] * std::sin(2.0f * PI * freq * ratio[k] * t) * expEnv(t, decay * dec[k]);
        b[i] = s * std::min(1.0f, t / 0.004f);
    }
    return b;
}

Buf padNote(float freq, float len, float attack, float release, float cutoff)  // slow, soft chord note (three detuned saws)
{
    Buf b = make(len);
    LowPass lp; lp.setCutoff(cutoff);
    float ph[3] = { 0.0f, 0.33f, 0.66f };
    const float det[3] = { 0.997f, 1.0f, 1.004f };
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR, s = 0.0f;
        for (int k = 0; k < 3; ++k) {
            ph[k] += freq * det[k] / SR; ph[k] -= std::floor(ph[k]);
            s += (2.0f * ph[k] - 1.0f);
        }
        float env = std::min(1.0f, t / attack) * std::min(1.0f, (len - t) / release);
        b[i] = lp.process(s / 3.0f) * env;
    }
    return b;
}

// ---------------------------------------------------------------- the sounds
Buf footstep(bool indoor, bool run, uint32_t seed)
{
    Rng rng(seed);
    Buf b = make(0.22f);
    LowPass body; body.setCutoff(indoor ? 2200.0f : 900.0f);
    HighPass tick; tick.setCutoff(3000.0f);
    float tau = run ? 0.045f : 0.032f, thumpHz = 80.0f + (float)(seed % 5) * 6.0f;
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        float s = body.process(rng.n()) * expEnv(t, tau) * 0.9f;
        s += std::sin(2.0f * PI * thumpHz * t) * expEnv(t, 0.05f) * (run ? 0.9f : 0.7f);        // heel
        if (indoor) s += tick.process(rng.n()) * expEnv(t, 0.004f) * 0.35f;                       // hard floor click
        else if (t < 0.09f && rng.f() < 0.03f) s += rng.n() * 0.5f;                               // gravel / grass crunch
        b[i] = s;
    }
    normalize(b, run ? 0.75f : 0.55f);
    fadeEdges(b, 0.001f, 0.01f);
    return b;
}

Buf pistolShot(uint32_t seed)
{
    Rng rng(seed);
    Buf dry = make(0.5f);
    HighPass crackHp; crackHp.setCutoff(1800.0f);
    LowPass bodyLp; bodyLp.setCutoff(3500.0f);
    float phase = 0.0f;
    for (size_t i = 0; i < dry.size(); ++i) {
        float t = (float)i / SR;
        float n = rng.n();
        float crack = crackHp.process(n) * expEnv(t, 0.006f) * 1.2f;                              // the sharp bang
        float body = bodyLp.process(n) * expEnv(t, 0.045f) * 0.9f;                                // the "whump"
        phase += lerp(190.0f, 45.0f, std::min(1.0f, t / 0.15f)) / SR;
        float boom = std::sin(2.0f * PI * phase) * expEnv(t, 0.07f) * 1.1f;                       // low punch
        dry[i] = crack + body + boom;
    }
    Buf out = reverb(dry, 0.55f, 0.74f, 0.9f);                                                    // the room tail
    LowPass tail; tail.setCutoff(5000.0f);
    for (float& x : out) x = std::tanh(1.4f * tail.process(x));
    normalize(out, 0.95f);
    fadeEdges(out, 0.0005f, 0.05f);
    return out;
}

Buf pistolReload()
{
    Buf b = make(1.6f);
    add(b, click(2600.0f, 0.012f, 0.06f, 3), 0.12f, 0.7f);                                        // magazine release
    add(b, thump(300.0f, 150.0f, 0.02f, 0.06f), 0.12f, 0.4f);
    add(b, noiseBurst(0.12f, 0.05f, 3000.0f, 900.0f, 5), 0.30f, 0.45f);                           // magazine slides out
    add(b, click(1700.0f, 0.02f, 0.08f, 7), 0.42f, 0.3f);
    add(b, thump(240.0f, 110.0f, 0.03f, 0.08f), 0.80f, 0.7f);                                     // new magazine in
    add(b, click(2300.0f, 0.015f, 0.07f, 9), 0.80f, 0.8f);
    add(b, noiseBurst(0.14f, 0.07f, 4000.0f, 1200.0f, 11), 1.12f, 0.5f);                          // slide pulled back
    add(b, click(1500.0f, 0.04f, 0.12f, 13), 1.28f, 0.9f);                                        // slide snaps forward
    add(b, thump(200.0f, 100.0f, 0.03f, 0.08f), 1.28f, 0.4f);
    normalize(b, 0.8f);
    return b;
}

Buf pistolEmpty()
{
    Buf b = click(1100.0f, 0.03f, 0.14f, 21);
    add(b, thump(180.0f, 120.0f, 0.02f, 0.05f), 0.0f, 0.4f);
    normalize(b, 0.6f);
    fadeEdges(b, 0.0005f, 0.02f);
    return b;
}

Buf bulletImpact()
{
    Buf b = noiseBurst(0.3f, 0.03f, 2600.0f, 300.0f, 31);
    add(b, click(3500.0f, 0.005f, 0.04f, 33), 0.0f, 0.5f);
    add(b, thump(140.0f, 80.0f, 0.04f, 0.15f), 0.0f, 0.5f);
    normalize(b, 0.6f);
    fadeEdges(b, 0.0005f, 0.05f);
    return b;
}

Buf jumpSound()
{
    Rng rng(41);
    Buf b = make(0.25f);
    BandPass bp; bp.set(1200.0f, 0.8f);
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        b[i] = bp.process(rng.n()) * std::min(1.0f, t / 0.02f) * expEnv(t, 0.07f);
    }
    normalize(b, 0.35f);
    fadeEdges(b, 0.002f, 0.04f);
    return b;
}

Buf landSound()
{
    Buf b = thump(95.0f, 45.0f, 0.07f, 0.3f);
    add(b, noiseBurst(0.2f, 0.045f, 700.0f, 60.0f, 43), 0.0f, 0.7f);
    normalize(b, 0.7f);
    fadeEdges(b, 0.001f, 0.05f);
    return b;
}

Buf playerHurt(int variant)
{
    VoiceParams p = { 0.45f, 165.0f - variant * 20.0f, 105.0f, 6.0f, 0.02f, 700.0f, 520.0f, 1150.0f, 900.0f, 6.0f,
                      0.02f, 0.18f, 0.0f, 0.0f, 0.25f, 2.0f };
    Buf b = formantVoice(p, 100 + variant);
    normalize(b, 0.8f);
    fadeEdges(b, 0.002f, 0.05f);
    return b;
}

Buf playerDeath()
{
    VoiceParams p = { 1.8f, 135.0f, 52.0f, 5.5f, 0.04f, 800.0f, 380.0f, 1300.0f, 700.0f, 6.0f, 0.05f, 0.7f, 18.0f, 0.15f, 0.2f, 2.2f };
    Buf b = formantVoice(p, 110);
    add(b, thump(70.0f, 38.0f, 0.1f, 0.4f), 1.45f, 0.9f);
    add(b, noiseBurst(0.3f, 0.07f, 500.0f, 50.0f, 112), 1.45f, 0.6f);
    normalize(b, 0.85f);
    fadeEdges(b, 0.002f, 0.1f);
    return b;
}

Buf zombieGroan(int variant)
{
    float base = 62.0f + variant * 13.0f;
    VoiceParams p = { 1.5f + 0.25f * (float)variant, base * 1.12f, base * 0.92f, 4.5f, 0.03f,
                      420.0f + variant * 40.0f, 620.0f, 900.0f, 1250.0f - variant * 80.0f, 7.0f,
                      0.35f, 0.6f, 28.0f, 0.35f, 0.22f, 2.4f };
    Buf b = formantVoice(p, 200 + variant);
    normalize(b, 0.8f);
    fadeEdges(b, 0.01f, 0.1f);
    return b;
}

Buf zombieAlert()
{
    VoiceParams p = { 1.1f, 210.0f, 175.0f, 7.0f, 0.06f, 900.0f, 750.0f, 1700.0f, 1300.0f, 6.0f, 0.03f, 0.5f, 35.0f, 0.4f, 0.3f, 3.5f };
    Buf b = formantVoice(p, 220);
    add(b, noiseBurst(0.25f, 0.1f, 5000.0f, 800.0f, 221), 0.0f, 0.4f);
    normalize(b, 0.85f);
    fadeEdges(b, 0.002f, 0.08f);
    return b;
}

Buf zombieStep()
{
    Rng rng(231);
    Buf b = make(0.2f);
    LowPass lp; lp.setCutoff(500.0f);
    BandPass drag; drag.set(900.0f, 0.7f);
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        float n = rng.n();
        b[i] = lp.process(n) * expEnv(t, 0.045f) + drag.process(n) * std::min(1.0f, t / 0.05f) * expEnv(t, 0.07f) * 0.35f;
    }
    normalize(b, 0.4f);
    fadeEdges(b, 0.002f, 0.04f);
    return b;
}

Buf zombieAttack()
{
    VoiceParams p = { 0.7f, 95.0f, 135.0f, 0.0f, 0.0f, 700.0f, 850.0f, 1100.0f, 1500.0f, 6.0f, 0.04f, 0.15f, 42.0f, 0.5f, 0.35f, 3.0f };
    Buf b = formantVoice(p, 240);
    add(b, noiseBurst(0.1f, 0.04f, 4000.0f, 600.0f, 241), 0.05f, 0.5f);
    add(b, noiseBurst(0.1f, 0.04f, 4000.0f, 600.0f, 242), 0.28f, 0.4f);
    normalize(b, 0.85f);
    fadeEdges(b, 0.002f, 0.05f);
    return b;
}

Buf zombieHit()
{
    VoiceParams p = { 0.35f, 115.0f, 68.0f, 0.0f, 0.0f, 520.0f, 400.0f, 950.0f, 700.0f, 6.0f, 0.01f, 0.2f, 22.0f, 0.3f, 0.2f, 2.5f };
    Buf b = formantVoice(p, 250);
    add(b, thump(130.0f, 60.0f, 0.04f, 0.15f), 0.0f, 0.8f);
    add(b, noiseBurst(0.12f, 0.03f, 1800.0f, 200.0f, 251), 0.0f, 0.5f);                          // wet impact
    normalize(b, 0.7f);
    fadeEdges(b, 0.001f, 0.05f);
    return b;
}

Buf zombieDeath()
{
    VoiceParams p = { 2.0f, 95.0f, 36.0f, 4.0f, 0.04f, 900.0f, 320.0f, 1300.0f, 600.0f, 7.0f, 0.05f, 1.2f, 24.0f, 0.4f, 0.25f, 2.4f };
    Buf b = formantVoice(p, 260);
    add(b, thump(65.0f, 35.0f, 0.1f, 0.45f), 1.55f, 0.9f);                                       // the body hits the floor
    add(b, noiseBurst(0.3f, 0.06f, 600.0f, 40.0f, 261), 1.55f, 0.6f);
    b.resize((size_t)(2.4f * SR), 0.0f);
    normalize(b, 0.9f);
    fadeEdges(b, 0.002f, 0.1f);
    return b;
}

// ---- ambience loops
Buf ambIndoor()
{
    const float len = 10.0f, fade = 1.0f;
    Rng rng(301);
    Buf b = make(len + fade);
    LowPass room; room.setCutoff(260.0f);
    LowPass rumble; rumble.setCutoff(70.0f);
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        float am = 1.0f + 0.2f * std::sin(2.0f * PI * 0.2f * t);                                  // the hum slowly breathes
        float hum = (0.12f * std::sin(2.0f * PI * 50.0f * t) + 0.08f * std::sin(2.0f * PI * 100.0f * t) + 0.03f * std::sin(2.0f * PI * 150.0f * t)) * am;
        float buzz = 0.012f * (std::sin(2.0f * PI * 120.0f * t) + 0.5f * std::sin(2.0f * PI * 240.0f * t) + 0.33f * std::sin(2.0f * PI * 360.0f * t));  // fluorescent tube
        b[i] = hum + buzz + room.process(rng.n()) * 0.5f + rumble.process(rng.n()) * 1.5f;
    }
    Buf out = makeLoop(b, fade);
    normalize(out, 0.55f);
    return out;
}

Buf ambWind()
{
    const float len = 12.0f, fade = 1.5f;
    Rng rng(311);
    Buf b = make(len + fade);
    LowPass lp; BandPass whistle; whistle.set(900.0f, 3.0f);
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        if ((i & 31) == 0) {
            lp.setCutoff(lerp(260.0f, 720.0f, 0.5f + 0.5f * std::sin(2.0f * PI * (1.0f / 6.0f) * t)));
            whistle.set(820.0f + 140.0f * std::sin(2.0f * PI * (1.0f / 12.0f) * t), 4.0f);
        }
        float gust = 0.55f + 0.45f * std::sin(2.0f * PI * 0.25f * t) * std::sin(2.0f * PI * (1.0f / 4.0f) * t + 1.0f);
        float n = rng.n();
        b[i] = lp.process(n) * (0.5f + 0.5f * gust) + whistle.process(n) * 0.18f * gust * gust;
    }
    Buf out = makeLoop(b, fade);
    normalize(out, 0.5f);
    return out;
}

Buf ambSiren()
{
    const float len = 16.0f, fade = 1.0f;
    Buf b = make(len + fade);
    float phase = 0.0f, phase2 = 0.0f;
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        float f = 650.0f + 420.0f * (0.5f - 0.5f * std::cos(2.0f * PI * t / 8.0f));              // the slow "wail": up and down
        phase += f / SR; phase -= std::floor(phase);
        float s = std::sin(2.0f * PI * phase) + 0.4f * std::sin(4.0f * PI * phase) + 0.2f * std::sin(6.0f * PI * phase);
        float yelp = (std::fmod(t, 1.4f) < 0.7f) ? 760.0f : 990.0f;                                // a second, two-tone siren far away
        phase2 += yelp / SR; phase2 -= std::floor(phase2);
        float gate = (std::fmod(t + 3.0f, 16.0f) < 7.0f) ? 0.0f : 1.0f;
        b[i] = s * 0.7f + gate * std::sin(2.0f * PI * phase2) * 0.35f;
    }
    LowPass dist; dist.setCutoff(1700.0f);                                                          // distance: soft and dull
    for (float& x : b) x = dist.process(x);
    Buf wet = reverb(b, 0.7f, 0.8f, 0.0f);
    wet.resize(b.size());
    Buf out = makeLoop(wet, fade);
    normalize(out, 0.5f);
    return out;
}

Buf electricZap()
{
    Rng rng(321);
    Buf b = make(0.3f);
    BandPass bp; bp.set(3200.0f, 1.2f);
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        float crackle = (rng.f() < 0.012f * expEnv(t, 0.1f) * 8.0f) ? rng.n() : 0.0f;               // random sparks that thin out
        float buzz = std::sin(2.0f * PI * 120.0f * t) * 0.5f + std::sin(2.0f * PI * 240.0f * t) * 0.3f;
        b[i] = bp.process(crackle * 3.0f) + buzz * expEnv(t, 0.08f) * 0.5f;
    }
    normalize(b, 0.5f);
    fadeEdges(b, 0.001f, 0.06f);
    return b;
}

Buf gateOpen()
{
    Rng rng(331);
    Buf b = make(2.9f);
    LowPass motor; motor.setCutoff(500.0f);
    BandPass rattle; rattle.set(1300.0f, 1.0f);
    float phase = 0.0f, ph2 = 0.0f;
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        float run = std::min(1.0f, t / 0.25f) * std::min(1.0f, (2.5f - t) / 0.3f);
        if (run < 0.0f) run = 0.0f;
        phase += lerp(52.0f, 72.0f, std::min(1.0f, t / 2.5f)) / SR; phase -= std::floor(phase);
        ph2 += (180.0f + 25.0f * std::sin(2.0f * PI * 0.7f * t)) / SR; ph2 -= std::floor(ph2);
        float chain = 0.5f + 0.5f * std::sin(2.0f * PI * 17.0f * t);                                // chain links rattling
        b[i] = (motor.process(2.0f * phase - 1.0f) * 0.9f + rattle.process(rng.n()) * 0.5f * chain + std::sin(2.0f * PI * ph2) * 0.12f) * run;
    }
    add(b, thump(110.0f, 50.0f, 0.06f, 0.3f), 2.55f, 0.9f);                                         // the gate bangs open
    add(b, click(900.0f, 0.1f, 0.4f, 333), 2.55f, 0.5f);
    normalize(b, 0.85f);
    fadeEdges(b, 0.005f, 0.2f);
    return b;
}

Buf tonePulse(float freq, float len, float amp)                                                    // soft electronic beep
{
    Buf b = make(len);
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        b[i] = std::sin(2.0f * PI * freq * t) * amp * std::min(1.0f, t / 0.005f) * std::min(1.0f, (len - t) / 0.02f);
    }
    return b;
}

Buf boothBeep()
{
    Buf b;
    add(b, tonePulse(1046.0f, 0.09f, 0.7f), 0.0f, 1.0f);
    add(b, tonePulse(1568.0f, 0.12f, 0.7f), 0.13f, 1.0f);
    b.resize((size_t)(0.35f * SR), 0.0f);
    normalize(b, 0.55f);
    return b;
}

Buf boothDenied()
{
    Buf b;
    for (int k = 0; k < 2; ++k) {                                                                  // two low square-ish buzzes
        Buf t = tonePulse(196.0f, 0.15f, 1.0f);
        for (float& x : t) x = std::tanh(4.0f * x) * 0.6f;
        add(b, t, 0.2f * (float)k, 1.0f);
    }
    b.resize((size_t)(0.45f * SR), 0.0f);
    normalize(b, 0.55f);
    return b;
}

Buf emergencyBroadcast()
{
    Rng rng(341);
    Buf b = make(7.0f);
    for (size_t i = 0; i < (size_t)(1.2f * SR); ++i) {                                             // the attention tone
        float t = (float)i / SR;
        b[i] = (std::sin(2.0f * PI * 853.0f * t) + std::sin(2.0f * PI * 960.0f * t)) * 0.3f * std::min(1.0f, (1.2f - t) / 0.02f) * std::min(1.0f, t / 0.01f);
    }
    BandPass radio; radio.set(1200.0f, 0.5f);
    LowPass lp; lp.setCutoff(3200.0f);
    HighPass hp; hp.setCutoff(300.0f);
    float syll = 0.0f; float syllTarget = 0.0f;
    for (size_t i = (size_t)(1.5f * SR); i < b.size(); ++i) {                                      // radio static with a speech-like rhythm
        float t = (float)i / SR;
        if ((i & 2047) == 0) syllTarget = (rng.f() < 0.7f) ? 0.4f + 0.6f * rng.f() : 0.05f;
        syll += (syllTarget - syll) * 0.003f;
        float n = hp.process(lp.process(rng.n()));
        float crackle = (rng.f() < 0.0008f) ? rng.n() * 0.8f : 0.0f;
        b[i] = (radio.process(n) * 0.8f * syll + n * 0.12f + crackle) * std::min(1.0f, (7.0f - t) / 0.2f);
    }
    normalize(b, 0.5f);
    return b;
}

// ---- interface and mission
Buf uiSelect()
{
    Buf b = tonePulse(1500.0f, 0.035f, 0.6f);
    add(b, click(3000.0f, 0.004f, 0.03f, 351), 0.0f, 0.3f);
    normalize(b, 0.4f);
    return b;
}

Buf uiConfirm()
{
    Buf b;
    add(b, tonePulse(620.0f, 0.07f, 0.7f), 0.0f, 1.0f);
    add(b, tonePulse(930.0f, 0.1f, 0.7f), 0.07f, 1.0f);
    normalize(b, 0.5f);
    return b;
}

Buf objectiveComplete()
{
    Buf b;
    const float notes[3] = { 523.25f, 659.25f, 783.99f };
    for (int k = 0; k < 3; ++k) add(b, bell(notes[k], 0.5f, 1.2f), 0.13f * (float)k, 0.7f);
    normalize(b, 0.6f);
    fadeEdges(b, 0.002f, 0.2f);
    return b;
}

Buf keycard()
{
    Rng rng(361);
    Buf b;
    add(b, tonePulse(1200.0f, 0.06f, 0.7f), 0.0f, 1.0f);
    add(b, tonePulse(1800.0f, 0.08f, 0.7f), 0.1f, 1.0f);
    Buf whoosh = make(0.35f);
    BandPass bp;
    for (size_t i = 0; i < whoosh.size(); ++i) {
        float t = (float)i / SR;
        if ((i & 15) == 0) bp.set(lerp(1500.0f, 4500.0f, t / 0.35f), 1.5f);
        whoosh[i] = bp.process(rng.n()) * std::sin(PI * t / 0.35f) * 0.35f;
    }
    add(b, whoosh, 0.0f, 0.6f);
    normalize(b, 0.55f);
    fadeEdges(b, 0.001f, 0.05f);
    return b;
}

Buf ammoPickup()
{
    Buf b;
    for (int k = 0; k < 3; ++k) add(b, click(1800.0f + 250.0f * (float)k, 0.015f, 0.08f, 371 + k), 0.07f * (float)k, 0.8f);
    add(b, bell(2200.0f, 0.15f, 0.3f), 0.2f, 0.25f);
    add(b, thump(160.0f, 90.0f, 0.03f, 0.08f), 0.0f, 0.5f);
    normalize(b, 0.55f);
    fadeEdges(b, 0.001f, 0.06f);
    return b;
}

Buf gateUnlocked()
{
    Buf b;
    add(b, thump(150.0f, 70.0f, 0.04f, 0.12f), 0.0f, 0.9f);
    add(b, click(1400.0f, 0.02f, 0.1f, 381), 0.0f, 0.7f);
    add(b, tonePulse(880.0f, 0.1f, 0.7f), 0.25f, 1.0f);
    add(b, tonePulse(1320.0f, 0.14f, 0.7f), 0.4f, 1.0f);
    add(b, bell(1320.0f, 0.35f, 0.7f), 0.55f, 0.4f);
    normalize(b, 0.65f);
    fadeEdges(b, 0.001f, 0.1f);
    return b;
}

Buf missionFailed()
{
    Rng rng(391);
    Buf b = make(4.5f);
    LowPass rumble; rumble.setCutoff(90.0f);
    float p1 = 0.0f, p2 = 0.0f;
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR;
        float env = std::min(1.0f, t / 0.05f) * expEnv(t, 1.6f);
        p1 += lerp(55.0f, 44.0f, t / 4.5f) / SR; p2 += lerp(82.5f, 63.0f, t / 4.5f) / SR;
        b[i] = (std::sin(2.0f * PI * p1) * 0.7f + std::sin(2.0f * PI * p2) * 0.4f + (2.0f * (p1 - std::floor(p1)) - 1.0f) * 0.1f
                + rumble.process(rng.n()) * 2.0f) * env;
    }
    add(b, thump(70.0f, 40.0f, 0.12f, 0.5f), 0.0f, 0.9f);                                           // two heart-beat thumps that fade
    add(b, thump(65.0f, 38.0f, 0.12f, 0.5f), 0.5f, 0.6f);
    add(b, bell(220.0f, 1.5f, 3.0f), 0.1f, 0.25f);
    LowPass dull; dull.setCutoff(900.0f);
    for (float& x : b) x = dull.process(x);
    normalize(b, 0.8f);
    fadeEdges(b, 0.001f, 0.8f);
    return b;
}

// ---- music
Buf menuMusic()
{
    const float len = 32.0f, fade = 2.0f;
    Rng rng(401);
    Buf b = make(len + fade);
    // the drone: three low notes that beat slowly against each other
    const float notes[3] = { 73.42f, 110.0f, 87.31f };           // D2, A2, F2: a dark minor sound
    LowPass sweep;
    for (size_t i = 0; i < b.size(); ++i) {
        float t = (float)i / SR, s = 0.0f;
        for (int k = 0; k < 3; ++k) {
            float f = notes[k] * (1.0f + 0.002f * (float)(k - 1) * std::sin(2.0f * PI * 0.1f * t));
            s += std::sin(2.0f * PI * f * t) * (0.3f - 0.07f * (float)k) * (0.8f + 0.2f * std::sin(2.0f * PI * (0.06f + 0.02f * (float)k) * t));
        }
        if ((i & 63) == 0) sweep.setCutoff(lerp(150.0f, 900.0f, 0.5f - 0.5f * std::cos(2.0f * PI * t / 16.0f)));
        float swell = 0.5f - 0.5f * std::cos(2.0f * PI * t / 16.0f);
        b[i] = s + sweep.process(rng.n()) * 0.35f * swell;
    }
    // sparse, distant bell notes
    const float when[5] = { 3.0f, 9.5f, 14.0f, 21.5f, 27.0f }, pitch[5] = { 293.66f, 220.0f, 349.23f, 261.63f, 196.0f };
    for (int k = 0; k < 5; ++k) add(b, bell(pitch[k], 2.2f, 6.0f), when[k], 0.22f);
    Buf wet = reverb(b, 0.45f, 0.8f, 0.0f);
    wet.resize(b.size());
    Buf out = makeLoop(wet, fade);
    normalize(out, 0.6f);
    return out;
}

Buf missionCompletedMusic()
{
    // four slow chords (Am - F - C - G) with a few quiet bell notes: sad but hopeful
    const float chord[4][3] = { {110.0f, 130.81f, 164.81f}, {87.31f, 130.81f, 174.61f}, {130.81f, 164.81f, 196.0f}, {98.0f, 146.83f, 196.0f} };
    Buf b = make(24.0f);
    for (int c = 0; c < 4; ++c)
        for (int k = 0; k < 3; ++k) add(b, padNote(chord[c][k] * (k == 2 ? 2.0f : 1.0f), 7.0f, 1.6f, 2.8f, 1100.0f), 5.0f * (float)c, 0.35f);
    const float bells[6] = { 440.0f, 523.25f, 659.25f, 587.33f, 523.25f, 392.0f };
    for (int k = 0; k < 6; ++k) add(b, bell(bells[k], 1.8f, 4.0f), 2.0f + 3.4f * (float)k, 0.2f);
    Buf wet = reverb(b, 0.5f, 0.8f, 2.0f);
    normalize(wet, 0.6f);
    fadeEdges(wet, 1.5f, 3.0f);
    return wet;
}

} // namespace

// ====================================================================== the table
static const SoundInfo kInfo[(int)Sound::Count] = {
    // name                 bus            loop   gain  maxV  ignorePause
    { "step_walk_indoor",   Bus::Sfx,      false, 0.55f, 4,  false },
    { "step_walk_outdoor",  Bus::Sfx,      false, 0.55f, 4,  false },
    { "step_run_indoor",    Bus::Sfx,      false, 0.70f, 4,  false },
    { "step_run_outdoor",   Bus::Sfx,      false, 0.70f, 4,  false },
    { "jump",               Bus::Sfx,      false, 0.55f, 2,  false },
    { "land",               Bus::Sfx,      false, 0.70f, 2,  false },
    { "pistol_shot",        Bus::Sfx,      false, 0.85f, 3,  false },
    { "pistol_reload",      Bus::Sfx,      false, 0.75f, 1,  false },
    { "pistol_empty",       Bus::Sfx,      false, 0.60f, 2,  false },
    { "bullet_impact",      Bus::Sfx,      false, 0.55f, 4,  false },
    { "player_hurt",        Bus::Sfx,      false, 0.80f, 2,  false },
    { "player_death",       Bus::Sfx,      false, 0.90f, 1,  false },
    { "zombie_groan",       Bus::Sfx,      false, 0.75f, 3,  false },
    { "zombie_alert",       Bus::Sfx,      false, 0.85f, 2,  false },
    { "zombie_step",        Bus::Sfx,      false, 0.45f, 4,  false },
    { "zombie_attack",      Bus::Sfx,      false, 0.85f, 3,  false },
    { "zombie_hit",         Bus::Sfx,      false, 0.75f, 3,  false },
    { "zombie_death",       Bus::Sfx,      false, 0.85f, 2,  false },
    { "amb_indoor",         Bus::Ambience, true,  0.55f, 1,  false },
    { "amb_wind",           Bus::Ambience, true,  0.55f, 1,  false },
    { "amb_siren",          Bus::Ambience, true,  0.45f, 1,  false },
    { "electric_zap",       Bus::Ambience, false, 0.45f, 2,  false },
    { "gate_open",          Bus::Sfx,      false, 0.85f, 1,  false },
    { "booth_beep",         Bus::Sfx,      false, 0.60f, 1,  false },
    { "booth_denied",       Bus::Sfx,      false, 0.60f, 1,  false },
    { "emergency_broadcast",Bus::Ambience, false, 0.60f, 1,  false },
    { "ui_select",          Bus::Sfx,      false, 0.55f, 2,  true  },
    { "ui_confirm",         Bus::Sfx,      false, 0.65f, 2,  true  },
    { "objective_complete", Bus::Sfx,      false, 0.65f, 1,  false },
    { "keycard",            Bus::Sfx,      false, 0.65f, 1,  false },
    { "ammo_pickup",        Bus::Sfx,      false, 0.65f, 1,  false },
    { "gate_unlocked",      Bus::Sfx,      false, 0.70f, 1,  false },
    { "mission_failed",     Bus::Sfx,      false, 0.85f, 1,  true  },
    { "menu_music",         Bus::Music,    true,  0.70f, 1,  true  },
    { "mission_completed",  Bus::Music,    false, 0.75f, 1,  true  },
};

const SoundInfo& soundInfo(Sound s) { return kInfo[(int)s]; }

std::vector<std::vector<float>> synthesize(Sound s)
{
    std::vector<Buf> v;
    switch (s) {
    case Sound::StepWalkIndoor:  for (int i = 0; i < 4; ++i) v.push_back(footstep(true, false, 11 + i)); break;
    case Sound::StepWalkOutdoor: for (int i = 0; i < 4; ++i) v.push_back(footstep(false, false, 21 + i)); break;
    case Sound::StepRunIndoor:   for (int i = 0; i < 4; ++i) v.push_back(footstep(true, true, 31 + i)); break;
    case Sound::StepRunOutdoor:  for (int i = 0; i < 4; ++i) v.push_back(footstep(false, true, 41 + i)); break;
    case Sound::Jump:            v.push_back(jumpSound()); break;
    case Sound::Land:            v.push_back(landSound()); break;
    case Sound::PistolShot:      for (int i = 0; i < 3; ++i) v.push_back(pistolShot(51 + i)); break;
    case Sound::PistolReload:    v.push_back(pistolReload()); break;
    case Sound::PistolEmpty:     v.push_back(pistolEmpty()); break;
    case Sound::BulletImpact:    v.push_back(bulletImpact()); break;
    case Sound::PlayerHurt:      for (int i = 0; i < 2; ++i) v.push_back(playerHurt(i)); break;
    case Sound::PlayerDeath:     v.push_back(playerDeath()); break;
    case Sound::ZombieGroan:     for (int i = 0; i < 4; ++i) v.push_back(zombieGroan(i)); break;
    case Sound::ZombieAlert:     v.push_back(zombieAlert()); break;
    case Sound::ZombieStep:      v.push_back(zombieStep()); break;
    case Sound::ZombieAttack:    v.push_back(zombieAttack()); break;
    case Sound::ZombieHit:       v.push_back(zombieHit()); break;
    case Sound::ZombieDeath:     v.push_back(zombieDeath()); break;
    case Sound::AmbIndoor:       v.push_back(ambIndoor()); break;
    case Sound::AmbWind:         v.push_back(ambWind()); break;
    case Sound::AmbSiren:        v.push_back(ambSiren()); break;
    case Sound::ElectricZap:     v.push_back(electricZap()); break;
    case Sound::GateOpen:        v.push_back(gateOpen()); break;
    case Sound::BoothBeep:       v.push_back(boothBeep()); break;
    case Sound::BoothDenied:     v.push_back(boothDenied()); break;
    case Sound::EmergencyBroadcast: v.push_back(emergencyBroadcast()); break;
    case Sound::UiSelect:        v.push_back(uiSelect()); break;
    case Sound::UiConfirm:       v.push_back(uiConfirm()); break;
    case Sound::ObjectiveComplete: v.push_back(objectiveComplete()); break;
    case Sound::KeycardCollected:  v.push_back(keycard()); break;
    case Sound::AmmoPickup:      v.push_back(ammoPickup()); break;
    case Sound::GateUnlocked:    v.push_back(gateUnlocked()); break;
    case Sound::MissionFailed:   v.push_back(missionFailed()); break;
    case Sound::MenuMusic:       v.push_back(menuMusic()); break;
    case Sound::MissionCompleted: v.push_back(missionCompletedMusic()); break;
    case Sound::Count: break;
    }
    return v;
}
