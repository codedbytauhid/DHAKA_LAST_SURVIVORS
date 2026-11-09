// src/audio/AudioManager.cpp
#include "audio/AudioManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

static const float kPi = 3.14159265f;

// ====================================================================== lock
struct AudioManager::Lock {
#ifdef _WIN32
    CRITICAL_SECTION cs;
    Lock() { InitializeCriticalSection(&cs); }
    ~Lock() { DeleteCriticalSection(&cs); }
    void lock() { EnterCriticalSection(&cs); }
    void unlock() { LeaveCriticalSection(&cs); }
#else
    void lock() {}
    void unlock() {}
#endif
};
// A scoped lock: locks here, unlocks when the function ends.
#define AUDIO_LOCK() struct ScopedLock { AudioManager::Lock* l; explicit ScopedLock(AudioManager::Lock* x) : l(x) { l->lock(); } ~ScopedLock() { l->unlock(); } } scopedLock(m_lock)

// ====================================================================== Windows sound card backend
#ifdef _WIN32
struct AudioManager::Backend {
    static const int kBuffers = 4;
    static const int kFrames = 1024;                 // 1024 frames = 23 ms; 4 buffers = about 93 ms of sound queued
    HWAVEOUT device = nullptr;
    WAVEHDR header[kBuffers];
    std::vector<short> data[kBuffers];
    std::vector<float> scratch;
    HANDLE thread = nullptr;
    volatile bool quit = false;
    AudioManager* owner = nullptr;
};

struct BackendAccess {
    static void fillAndQueue(AudioManager::Backend* b, int i)
    {
        b->owner->mix(b->scratch.data(), AudioManager::Backend::kFrames);
        for (int k = 0; k < AudioManager::Backend::kFrames * 2; ++k) {
            float x = std::max(-1.0f, std::min(1.0f, b->scratch[k]));
            b->data[i][k] = (short)(x * 32000.0f);
        }
        waveOutWrite(b->device, &b->header[i], sizeof(WAVEHDR));
    }
    static DWORD WINAPI threadMain(LPVOID param)
    {
        AudioManager::Backend* b = (AudioManager::Backend*)param;
        for (int i = 0; i < AudioManager::Backend::kBuffers; ++i) fillAndQueue(b, i);
        while (!b->quit) {
            for (int i = 0; i < AudioManager::Backend::kBuffers; ++i)
                if (b->header[i].dwFlags & WHDR_DONE) fillAndQueue(b, i);      // this buffer finished playing: refill it
            Sleep(3);
        }
        return 0;
    }
};

static AudioManager::Backend* openBackend(AudioManager* owner)
{
    AudioManager::Backend* b = new AudioManager::Backend();
    b->owner = owner;
    WAVEFORMATEX fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = kSampleRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = (WORD)(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    if (waveOutOpen(&b->device, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) { delete b; return nullptr; }
    b->scratch.assign(AudioManager::Backend::kFrames * 2, 0.0f);
    for (int i = 0; i < AudioManager::Backend::kBuffers; ++i) {
        b->data[i].assign(AudioManager::Backend::kFrames * 2, 0);
        std::memset(&b->header[i], 0, sizeof(WAVEHDR));
        b->header[i].lpData = (LPSTR)b->data[i].data();
        b->header[i].dwBufferLength = (DWORD)(b->data[i].size() * sizeof(short));
        waveOutPrepareHeader(b->device, &b->header[i], sizeof(WAVEHDR));
    }
    b->thread = CreateThread(nullptr, 0, BackendAccess::threadMain, b, 0, nullptr);
    if (!b->thread) { waveOutClose(b->device); delete b; return nullptr; }
    return b;
}

static void closeBackend(AudioManager::Backend* b)
{
    if (!b) return;
    b->quit = true;
    WaitForSingleObject(b->thread, 2000);
    CloseHandle(b->thread);
    waveOutReset(b->device);
    for (int i = 0; i < AudioManager::Backend::kBuffers; ++i) waveOutUnprepareHeader(b->device, &b->header[i], sizeof(WAVEHDR));
    waveOutClose(b->device);
    delete b;
}
#else
struct AudioManager::Backend {};
static AudioManager::Backend* openBackend(AudioManager*) { return nullptr; }     // no sound card access outside Windows
static void closeBackend(AudioManager::Backend*) {}
#endif

// ====================================================================== setup
AudioManager::AudioManager()
{
    for (int i = 0; i < (int)Sound::Count; ++i) m_lastVariant[i] = -1;
    m_voices.resize(40);
    m_lock = new Lock();
}

AudioManager::~AudioManager()
{
    shutdown();
    delete m_lock;
}

bool AudioManager::deviceOpen() const { return m_backend != nullptr; }

bool AudioManager::init(bool openDevice, const std::string& wavDir)
{
    for (int i = 0; i < (int)Sound::Count; ++i) {
        const SoundInfo& info = soundInfo((Sound)i);
        m_bank[i] = synthesize((Sound)i);

        // optional replacement files: <name>.wav, or <name>_1.wav, <name>_2.wav ... for several variations
        std::vector<std::vector<float>> files;
        std::vector<float> w;
        if (loadWav(wavDir + "/" + info.name + ".wav", w)) files.push_back(w);
        else
            for (int k = 1; k <= 8; ++k)
                if (loadWav(wavDir + "/" + info.name + "_" + std::to_string(k) + ".wav", w)) files.push_back(w);
        if (!files.empty()) {
            m_bank[i] = files;
            std::printf("[Audio] using your WAV file(s) for '%s' (%d)\n", info.name, (int)files.size());
        }
    }
    m_ready = true;
    if (openDevice) {
        m_backend = openBackend(this);
        if (!m_backend) std::fprintf(stderr, "[Audio] no sound device could be opened: the game will run silently.\n");
    }
    return m_ready;
}

void AudioManager::shutdown()
{
    if (m_backend) { closeBackend(m_backend); m_backend = nullptr; }     // stops the audio thread first
}

// ====================================================================== settings
void AudioManager::setVolumes(float master, float music, float sfx, float ambience)
{
    AUDIO_LOCK();
    m_masterTarget = std::max(0.0f, std::min(1.0f, master));
    m_busTarget[(int)Bus::Music] = std::max(0.0f, std::min(1.0f, music));
    m_busTarget[(int)Bus::Sfx] = std::max(0.0f, std::min(1.0f, sfx));
    m_busTarget[(int)Bus::Ambience] = std::max(0.0f, std::min(1.0f, ambience));
}

void AudioManager::setListener(const glm::vec3& pos, const glm::vec3& right)
{
    AUDIO_LOCK();
    m_listenerPos = pos;
    m_listenerRight = right;
}

void AudioManager::setPaused(bool paused)
{
    AUDIO_LOCK();
    m_paused = paused;
}

// ====================================================================== voices
unsigned AudioManager::nextRandom()
{
    m_rng ^= m_rng << 13; m_rng ^= m_rng >> 17; m_rng ^= m_rng << 5;
    return m_rng;
}

float AudioManager::fadeToSmooth(float seconds) const
{
    if (seconds <= 0.002f) return 1.0f;
    return 1.0f - std::exp(-1.0f / (seconds * (float)kSampleRate * 0.35f));     // per-sample smoothing: reaches ~97% in `seconds`
}

AudioManager::Voice* AudioManager::find(int handle)
{
    for (Voice& v : m_voices) if (v.active && v.handle == handle) return &v;
    return nullptr;
}
const AudioManager::Voice* AudioManager::find(int handle) const
{
    for (const Voice& v : m_voices) if (v.active && v.handle == handle) return &v;
    return nullptr;
}

int AudioManager::startVoice(Sound s, float volume, float pitch, bool loop, float fadeIn)
{
    if (!m_ready || m_bank[(int)s].empty()) return -1;
    const SoundInfo& info = soundInfo(s);

    // 1. never more than maxVoices copies of the same sound: fade out the oldest one
    int same = 0; Voice* oldest = nullptr;
    for (Voice& v : m_voices)
        if (v.active && !v.stopping && v.sound == s) { ++same; if (!oldest || v.serial < oldest->serial) oldest = &v; }
    if (same >= info.maxVoices && oldest) { oldest->stopping = true; oldest->target = 0.0f; oldest->smooth = fadeToSmooth(0.05f); }

    // 2. find a free slot, or take over the oldest short sound effect
    Voice* slot = nullptr;
    for (Voice& v : m_voices) if (!v.active) { slot = &v; break; }
    if (!slot) {
        for (Voice& v : m_voices)
            if (!v.loop && v.bus == Bus::Sfx && (!slot || v.serial < slot->serial)) slot = &v;
        if (!slot) return -1;
    }

    // 3. choose a variation, but not the same one twice in a row
    int variants = (int)m_bank[(int)s].size(), pick = 0;
    if (variants > 1) {
        pick = (int)(nextRandom() % (unsigned)variants);
        if (pick == m_lastVariant[(int)s]) pick = (pick + 1) % variants;
        m_lastVariant[(int)s] = pick;
    }

    Voice& v = *slot;
    v = Voice();
    v.active = true;
    v.handle = m_nextHandle++;
    v.serial = ++m_serial;
    v.sound = s;
    v.bus = info.bus;
    v.ignorePause = info.ignorePause;
    v.data = &m_bank[(int)s][pick];
    v.loop = loop;
    v.pitch = std::max(0.5f, std::min(2.0f, pitch));
    v.target = std::max(0.0f, volume) * info.gain;
    v.gain = (fadeIn > 0.0f) ? 0.0f : v.target;
    v.smooth = (fadeIn > 0.0f) ? fadeToSmooth(fadeIn) : 1.0f;
    if (loop) v.pos = (double)(nextRandom() % (unsigned)v.data->size());      // loops start at a random point: no two runs sound identical
    return v.handle;
}

int AudioManager::play(Sound s, float volume, float pitch, float pan)
{
    AUDIO_LOCK();
    int h = startVoice(s, volume, pitch, false, 0.0f);
    if (Voice* v = find(h)) v->pan = std::max(-1.0f, std::min(1.0f, pan));
    return h;
}

int AudioManager::playAt(Sound s, const glm::vec3& pos, float volume, float pitch, bool muffled)
{
    AUDIO_LOCK();
    int h = startVoice(s, volume, pitch, false, 0.0f);
    if (Voice* v = find(h)) { v->positional = true; v->worldPos = pos; v->muffled = muffled; }
    return h;
}

int AudioManager::playLoop(Sound s, float volume, float fadeIn)
{
    AUDIO_LOCK();
    return startVoice(s, volume, 1.0f, true, fadeIn);
}

void AudioManager::setVoiceVolume(int handle, float volume, float fadeSeconds)
{
    AUDIO_LOCK();
    if (Voice* v = find(handle)) { v->target = std::max(0.0f, volume) * soundInfo(v->sound).gain; v->smooth = fadeToSmooth(fadeSeconds); }
}

void AudioManager::stop(int handle, float fadeSeconds)
{
    AUDIO_LOCK();
    if (Voice* v = find(handle)) { v->stopping = true; v->target = 0.0f; v->smooth = fadeToSmooth(fadeSeconds); }
}

void AudioManager::stopBus(Bus bus, float fadeSeconds)
{
    AUDIO_LOCK();
    for (Voice& v : m_voices)
        if (v.active && v.bus == bus) { v.stopping = true; v.target = 0.0f; v.smooth = fadeToSmooth(fadeSeconds); }
}

void AudioManager::stopAll(float fadeSeconds)
{
    AUDIO_LOCK();
    for (Voice& v : m_voices)
        if (v.active) { v.stopping = true; v.target = 0.0f; v.smooth = fadeToSmooth(fadeSeconds); }
}

bool AudioManager::isPlaying(int handle) const
{
    AUDIO_LOCK();
    const Voice* v = find(handle);
    return v && !v->stopping;
}

int AudioManager::activeVoices() const
{
    AUDIO_LOCK();
    int n = 0;
    for (const Voice& v : m_voices) if (v.active) ++n;
    return n;
}

// ====================================================================== the mixer
void AudioManager::mix(float* out, int frames)
{
    AUDIO_LOCK();
    std::fill(out, out + frames * 2, 0.0f);
    if (!m_ready) return;

    // volume changes glide over about 50 ms so moving a slider never clicks
    float k = 1.0f - std::exp(-(float)frames / ((float)kSampleRate * 0.05f));
    m_masterGain += (m_masterTarget - m_masterGain) * k;
    for (int b = 0; b < 3; ++b) m_busGain[b] += (m_busTarget[b] - m_busGain[b]) * k;

    for (Voice& v : m_voices) {
        if (!v.active) continue;
        bool frozen = m_paused && v.bus != Bus::Music && !v.ignorePause;
        if (frozen) continue;                                                   // pause: the sound simply waits

        float busGain = m_busGain[(int)v.bus] * ((m_paused && v.bus == Bus::Music) ? 0.45f : 1.0f);

        // stereo placement
        float atten = 1.0f, pan = v.pan, lpA = 1.0f;
        if (v.positional) {
            glm::vec3 d = v.worldPos - m_listenerPos;
            float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            const float ref = 2.5f, maxDist = 42.0f;
            float t = std::max(0.0f, std::min(1.0f, (dist - ref) / (maxDist - ref)));
            atten = (1.0f - t) * (1.0f - t);
            pan = (dist > 0.2f) ? (d.x * m_listenerRight.x + d.y * m_listenerRight.y + d.z * m_listenerRight.z) / dist * 0.85f : 0.0f;
            if (v.muffled) { atten *= 0.55f; lpA = 0.09f; }                       // heard through a wall: quieter and dull
            else if (dist > 14.0f) lpA = 0.45f;                                   // far away: a little duller
        }
        float angle = (pan + 1.0f) * kPi * 0.25f;
        float gl = std::cos(angle) * busGain * atten, gr = std::sin(angle) * busGain * atten;

        const std::vector<float>& d = *v.data;
        const double n = (double)d.size();
        for (int f = 0; f < frames; ++f) {
            v.gain += (v.target - v.gain) * v.smooth;
            size_t i0 = (size_t)v.pos;
            float frac = (float)(v.pos - (double)i0);
            size_t i1 = i0 + 1;
            if (i1 >= d.size()) i1 = v.loop ? 0 : d.size() - 1;
            float s = d[i0] + (d[i1] - d[i0]) * frac;
            if (lpA < 1.0f) { v.lp += lpA * (s - v.lp); s = v.lp; }
            s *= v.gain;
            out[f * 2] += s * gl;
            out[f * 2 + 1] += s * gr;
            v.pos += v.pitch;
            if (v.pos >= n) {
                if (v.loop) v.pos -= n; else { v.active = false; break; }
            }
        }
        if (v.active && v.stopping && v.gain < 0.003f) v.active = false;        // faded out (below -50 dB)
    }

    // master volume + a soft limiter: loud moments are squeezed smoothly instead of crackling
    for (int i = 0; i < frames * 2; ++i) {
        float x = out[i] * m_masterGain * 0.8f;
        float a = std::fabs(x);
        if (a > 0.6f) x = (x < 0 ? -1.0f : 1.0f) * (0.6f + 0.4f * std::tanh((a - 0.6f) / 0.4f));
        out[i] = x;
    }
}

// ====================================================================== WAV files
static void put16(std::ofstream& f, unsigned v) { char b[2] = { (char)(v & 255), (char)((v >> 8) & 255) }; f.write(b, 2); }
static void put32(std::ofstream& f, unsigned v) { char b[4] = { (char)(v & 255), (char)((v >> 8) & 255), (char)((v >> 16) & 255), (char)((v >> 24) & 255) }; f.write(b, 4); }

bool AudioManager::writeWav(const std::string& path, const std::vector<float>& mono, int rate)
{
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    unsigned bytes = (unsigned)mono.size() * 2;
    f.write("RIFF", 4); put32(f, 36 + bytes); f.write("WAVE", 4);
    f.write("fmt ", 4); put32(f, 16); put16(f, 1); put16(f, 1); put32(f, (unsigned)rate); put32(f, (unsigned)rate * 2); put16(f, 2); put16(f, 16);
    f.write("data", 4); put32(f, bytes);
    for (float x : mono) {
        float c = std::max(-1.0f, std::min(1.0f, x));
        short s = (short)std::lrintf(c * 32767.0f);
        put16(f, (unsigned short)s);
    }
    return f.good();
}

static unsigned rd16(const unsigned char* p) { return p[0] | (p[1] << 8); }
static unsigned rd32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }

bool AudioManager::loadWav(const std::string& path, std::vector<float>& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    std::vector<unsigned char> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (b.size() < 44 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "WAVE", 4) != 0) return false;

    unsigned format = 0, channels = 0, rate = 0, bits = 0;
    const unsigned char* data = nullptr; size_t dataBytes = 0;
    size_t p = 12;
    while (p + 8 <= b.size()) {                                      // walk through the chunks
        unsigned size = rd32(&b[p + 4]);
        const unsigned char* body = &b[p + 8];
        if (std::memcmp(&b[p], "fmt ", 4) == 0 && size >= 16 && p + 8 + 16 <= b.size()) {
            format = rd16(body); channels = rd16(body + 2); rate = rd32(body + 4); bits = rd16(body + 14);
        } else if (std::memcmp(&b[p], "data", 4) == 0) {
            data = body; dataBytes = std::min<size_t>(size, b.size() - (p + 8));
            break;
        }
        p += 8 + size + (size & 1);
    }
    if (!data || channels < 1 || rate == 0) return false;
    if (!((format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32))) return false;

    size_t bytesPer = bits / 8, frames = dataBytes / (bytesPer * channels);
    std::vector<float> mono(frames);
    for (size_t i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (unsigned c = 0; c < channels; ++c) {
            const unsigned char* s = data + (i * channels + c) * bytesPer;
            float x = 0.0f;
            if (format == 3) { float v; std::memcpy(&v, s, 4); x = v; }
            else if (bits == 8) x = ((int)s[0] - 128) / 128.0f;
            else if (bits == 16) x = (short)rd16(s) / 32768.0f;
            else if (bits == 24) { int v = (s[0] << 8) | (s[1] << 16) | (s[2] << 24); x = (v >> 8) / 8388608.0f; }
            else x = (int)rd32(s) / 2147483648.0f;
            sum += x;
        }
        mono[i] = sum / (float)channels;
    }
    if ((int)rate == kSampleRate) { out = mono; return true; }
    size_t n = (size_t)((double)frames * kSampleRate / rate);        // resample (linear) to 44.1 kHz
    out.assign(n, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        double src = (double)i * rate / kSampleRate;
        size_t i0 = (size_t)src; double fr = src - (double)i0;
        size_t i1 = std::min(i0 + 1, frames - 1);
        out[i] = (float)(mono[std::min(i0, frames - 1)] * (1.0 - fr) + mono[i1] * fr);
    }
    return true;
}

int AudioManager::exportWavs(const std::string& dir) const
{
    int written = 0;
    for (int i = 0; i < (int)Sound::Count; ++i) {
        const SoundInfo& info = soundInfo((Sound)i);
        for (size_t k = 0; k < m_bank[i].size(); ++k) {
            std::string name = dir + "/" + info.name + (m_bank[i].size() > 1 ? "_" + std::to_string(k + 1) : std::string()) + ".wav";
            if (writeWav(name, m_bank[i][k], kSampleRate)) ++written;
            else { std::fprintf(stderr, "[Audio] could not write %s (does the folder exist?)\n", name.c_str()); return written; }
        }
    }
    return written;
}
