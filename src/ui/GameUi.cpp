// src/ui/GameUi.cpp
#include "ui/GameUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using glm::vec4;

namespace {
const vec4 kWhite(0.95f, 0.95f, 0.95f, 1.0f);
const vec4 kGrey(0.65f, 0.65f, 0.68f, 1.0f);
const vec4 kRed(0.90f, 0.15f, 0.10f, 1.0f);
const vec4 kGreen(0.25f, 0.95f, 0.45f, 1.0f);
const vec4 kYellow(1.00f, 0.85f, 0.25f, 1.0f);

vec4 withAlpha(vec4 c, float a) { c.a *= a; return c; }
float saturate(float v) { return std::max(0.0f, std::min(1.0f, v)); }
float reveal(float t, float delay, float fade = 0.7f) { return saturate((t - delay) / fade); }   // staggered fade-in
}

const std::vector<std::string>& GameUi::mainMenuItems()
{
    static const std::vector<std::string> items = { "START MISSION", "CONTROLS", "SETTINGS", "EXIT" };
    return items;
}

const std::vector<std::string>& GameUi::pauseMenuItems()
{
    static const std::vector<std::string> items = { "RESUME", "CONTROLS", "SETTINGS", "RESTART MISSION", "MAIN MENU", "EXIT" };
    return items;
}

void GameUi::vignette(float strength) const
{
    float w = (float)m_hud.width(), h = (float)m_hud.height();
    for (int i = 0; i < 6; ++i) {                    // six nested bands: the nearer the edge, the darker
        float bw = (float)(i + 1) * 0.022f * std::min(w, h * 1.6f);
        float a = strength * 0.10f;
        m_hud.rect(0.0f, 0.0f, bw, h, vec4(0, 0, 0, a));
        m_hud.rect(w - bw, 0.0f, bw, h, vec4(0, 0, 0, a));
        m_hud.rect(0.0f, 0.0f, w, bw * 0.7f, vec4(0, 0, 0, a));
        m_hud.rect(0.0f, h - bw * 0.7f, w, bw * 0.7f, vec4(0, 0, 0, a));
    }
}

void GameUi::menuList(const std::vector<std::string>& items, int selected, float y, float px) const
{
    float cx = m_hud.width() * 0.5f, u = ui();
    for (size_t i = 0; i < items.size(); ++i) {
        bool sel = (int)i == selected;
        float iy = y + (float)i * px * 11.0f;
        if (sel) {                                   // a soft highlight bar behind the chosen item
            float bw = 460.0f * u, bh = px * 9.5f;
            m_hud.rect(cx - bw * 0.5f, iy - px * 1.2f, bw, bh, vec4(0.55f, 0.05f, 0.04f, 0.35f));
            m_hud.rect(cx - bw * 0.5f, iy - px * 1.2f, 4.0f * u, bh, vec4(0.95f, 0.2f, 0.1f, 0.9f));
        }
        m_hud.textCentered(items[i], cx, iy, px, sel ? kYellow : kGrey);
    }
}

// ---------------------------------------------------------------- main menu
void GameUi::mainMenu(int selected, float t) const
{
    float u = ui(), w = (float)m_hud.width(), h = (float)m_hud.height();
    m_hud.fullscreen(vec4(0.0f, 0.0f, 0.0f, 0.50f));
    vignette(1.4f);
    float flick = (std::fmod(t, 7.0f) > 6.75f) ? 0.35f : (0.88f + 0.12f * (float)std::sin(t * 2.3));      // the title flickers like a failing sign
    m_hud.textCentered("DHAKA: THE LAST SURVIVORS", w * 0.5f, h * 0.12f, 7.0f * u, withAlpha(kRed, flick));
    m_hud.textCentered("THE FALL OF BARIDHARA", w * 0.5f, h * 0.12f + 72.0f * u, 4.0f * u, kWhite);
    m_hud.textCentered("17 NOVEMBER 2027  -  DHAKA, BANGLADESH", w * 0.5f, h * 0.12f + 118.0f * u, 2.0f * u, kGrey);
    menuList(mainMenuItems(), selected, h * 0.44f, 4.0f * u);
    m_hud.textCentered("W / S OR UP / DOWN: CHOOSE      ENTER: CONFIRM", w * 0.5f, h - 40.0f * u, 2.0f * u, kGrey);
}

void GameUi::controls() const
{
    float u = ui(), w = (float)m_hud.width(), h = (float)m_hud.height();
    m_hud.fullscreen(vec4(0.0f, 0.0f, 0.0f, 0.82f));
    vignette(1.0f);
    m_hud.textCentered("CONTROLS", w * 0.5f, h * 0.07f, 6.0f * u, kWhite);
    const char* rows[][2] = {
        {"W A S D",      "MOVE"},
        {"MOUSE",        "LOOK AROUND"},
        {"SHIFT",        "SPRINT"},
        {"SPACE",        "JUMP"},
        {"RIGHT MOUSE",  "AIM (ZOOM AND RAISE THE PISTOL)"},
        {"LEFT MOUSE",   "SHOOT"},
        {"R",            "RELOAD"},
        {"E",            "INTERACT (CARD, AMMUNITION, PANEL, GATE)"},
        {"MOUSE WHEEL",  "CAMERA DISTANCE"},
        {"TAB",          "FREE / CAPTURE THE MOUSE"},
        {"ESC",          "PAUSE"},
        {"F1",           "DEBUG MODE (K KILL, G GOD, U CARD, 1-6 TELEPORT)"},
    };
    float px = 2.6f * u, x = w * 0.5f - 400.0f * u, y = h * 0.21f;
    for (auto& r : rows) {
        m_hud.textShadow(r[0], x, y, px, kYellow);
        m_hud.textShadow(r[1], x + 250.0f * u, y, px, kWhite);
        y += 32.0f * u;
    }
    m_hud.textCentered("PRESS ESC OR ENTER TO GO BACK", w * 0.5f, h - 46.0f * u, 2.5f * u, kGrey);
}

void GameUi::settings(int row, const Settings& s) const
{
    float u = ui(), w = (float)m_hud.width(), h = (float)m_hud.height();
    m_hud.fullscreen(vec4(0.0f, 0.0f, 0.0f, 0.82f));
    vignette(1.0f);
    m_hud.textCentered("SETTINGS", w * 0.5f, h * 0.10f, 6.0f * u, kWhite);

    const char* labels[kSettingsRows] = { "MASTER VOLUME", "MUSIC VOLUME", "SOUND EFFECTS", "AMBIENCE", "MOUSE SENSITIVITY" };
    float values[kSettingsRows] = { s.masterVolume, s.musicVolume, s.sfxVolume, s.ambienceVolume,
                                    (s.mouseSensitivity - Settings::kMinSensitivity) / (Settings::kMaxSensitivity - Settings::kMinSensitivity) };
    float px = 3.0f * u, x = w * 0.5f - 380.0f * u, y = h * 0.28f, barW = 300.0f * u, barH = 16.0f * u;
    for (int i = 0; i < kSettingsRows; ++i) {
        bool sel = (i == row);
        float ry = y + (float)i * 62.0f * u;
        if (sel) m_hud.rect(x - 20.0f * u, ry - 12.0f * u, 780.0f * u, 48.0f * u, vec4(0.55f, 0.05f, 0.04f, 0.35f));
        m_hud.textShadow(labels[i], x, ry, px, sel ? kYellow : kGrey);
        float bx = x + 360.0f * u;
        m_hud.rect(bx - 2.0f * u, ry - 2.0f * u, barW + 4.0f * u, barH + 4.0f * u, vec4(0.0f, 0.0f, 0.0f, 0.7f));
        m_hud.rect(bx, ry, barW * saturate(values[i]), barH, sel ? vec4(1.0f, 0.8f, 0.2f, 0.95f) : vec4(0.6f, 0.6f, 0.65f, 0.9f));
        char buf[24];
        if (i < 4) std::snprintf(buf, sizeof(buf), "%d%%", (int)std::lround(values[i] * 100.0f));
        else std::snprintf(buf, sizeof(buf), "%.2f", s.mouseSensitivity);
        m_hud.textShadow(buf, bx + barW + 18.0f * u, ry - 2.0f * u, px, sel ? kYellow : kWhite);
    }
    m_hud.textCentered("UP / DOWN: CHOOSE     LEFT / RIGHT: CHANGE     ESC OR ENTER: BACK", w * 0.5f, h - 56.0f * u, 2.2f * u, kGrey);
    m_hud.textCentered("CHANGES ARE APPLIED AND SAVED AUTOMATICALLY", w * 0.5f, h - 30.0f * u, 1.8f * u, vec4(0.5f, 0.5f, 0.55f, 1.0f));
}

// ---------------------------------------------------------------- intro (black screen, news, title)
void GameUi::intro(const MissionManager& m) const
{
    float u = ui(), w = (float)m_hud.width(), h = (float)m_hud.height();
    float t = m.introTime();
    m_hud.fullscreen(vec4(0.0f, 0.0f, 0.0f, 1.0f));
    m_hud.rect(0.0f, 0.0f, w, h * 0.09f, vec4(0.03f, 0.03f, 0.03f, 1.0f));              // cinema bars
    m_hud.rect(0.0f, h * 0.91f, w, h * 0.09f, vec4(0.03f, 0.03f, 0.03f, 1.0f));

    struct Line { float start, end; };
    auto alphaOf = [&](const Line& l) {
        if (t < l.start || t > l.end) return 0.0f;
        return saturate(std::min((t - l.start) / 0.6f, (l.end - t) / 0.6f));
    };

    float a;
    a = alphaOf({0.8f, 4.2f});
    if (a > 0.0f) {
        m_hud.textCentered("17 NOVEMBER 2027", w * 0.5f, h * 0.40f, 6.0f * u, withAlpha(kWhite, a));
        m_hud.textCentered("DHAKA, BANGLADESH", w * 0.5f, h * 0.40f + 70.0f * u, 3.0f * u, withAlpha(kGrey, a));
        m_hud.textCentered("(DISTANT SIRENS)", w * 0.5f, h * 0.84f, 2.2f * u, withAlpha(kGrey, a * 0.8f));
    }
    a = alphaOf({4.8f, 8.4f});
    if (a > 0.0f) {
        m_hud.textCentered("BARIDHARA UNIVERSITY", w * 0.5f, h * 0.40f, 5.0f * u, withAlpha(kWhite, a));
        m_hud.textCentered("LATE EVENING LABORATORY SESSION", w * 0.5f, h * 0.40f + 62.0f * u, 3.0f * u, withAlpha(kGrey, a));
    }
    a = alphaOf({9.0f, 13.4f});
    if (a > 0.0f) {                                                     // the emergency news broadcast
        float bw = 360.0f * u, bh = 46.0f * u, bx = w * 0.5f - bw * 0.5f, by = h * 0.33f;
        m_hud.rect(bx, by, bw, bh, vec4(0.75f, 0.05f, 0.05f, a));
        m_hud.textCentered("BREAKING NEWS", w * 0.5f, by + 9.0f * u, 4.0f * u, vec4(1.0f, 1.0f, 1.0f, a));
        m_hud.textCentered("A DANGEROUS INFECTION IS SPREADING ACROSS DHAKA.", w * 0.5f, by + 84.0f * u, 2.5f * u, withAlpha(kWhite, a));
        m_hud.textCentered("STAY INDOORS. DO NOT APPROACH THE INFECTED.", w * 0.5f, by + 116.0f * u, 2.5f * u, withAlpha(kWhite, a));
        m_hud.textCentered("(EMERGENCY BROADCAST)", w * 0.5f, h * 0.84f, 2.2f * u, withAlpha(kGrey, a * 0.8f));
    }
    a = alphaOf({13.8f, 16.0f});
    if (a > 0.0f) {
        m_hud.textCentered("DHAKA: THE LAST SURVIVORS", w * 0.5f, h * 0.40f, 5.0f * u, withAlpha(kRed, a));
        m_hud.textCentered("THE FALL OF BARIDHARA", w * 0.5f, h * 0.40f + 62.0f * u, 3.0f * u, withAlpha(kWhite, a));
    }
    m_hud.text("PRESS ENTER TO SKIP", w - m_hud.textWidth("PRESS ENTER TO SKIP", 2.0f * u) - 24.0f * u, h - 36.0f * u, 2.0f * u, kGrey);
}

// ---------------------------------------------------------------- in-game HUD
void GameUi::playing(const MissionManager& m, const PlayingInfo& info) const
{
    float u = ui(), w = (float)m_hud.width(), h = (float)m_hud.height();

    // damage feedback + a soft vignette that gets stronger when hurt
    vignette(0.5f + 1.5f * (1.0f - info.healthFraction));
    if (info.hurtFlash > 0.0f) m_hud.fullscreen(vec4(0.8f, 0.0f, 0.0f, 0.40f * info.hurtFlash));
    if (info.healthFraction < 0.3f && info.healthFraction > 0.0f)
        m_hud.fullscreen(vec4(0.6f, 0.0f, 0.0f, 0.08f + 0.05f * (float)std::sin(info.time * 5.0)));

    // objective panel, top-left
    std::string head = "MISSION: THE FALL OF BARIDHARA";
    std::string obj = m.objectiveText();
    std::string hint = m.objectiveHint();
    float pxS = 2.0f * u, pxM = 2.5f * u;
    float panelW = std::max(std::max(m_hud.textWidth(head, pxS), m_hud.textWidth(obj, pxM + 0.5f * u)), m_hud.textWidth(hint, 1.7f * u)) + 28.0f * u;
    float x = 18.0f * u, y = 16.0f * u;
    m_hud.rect(x, y, panelW, 112.0f * u, vec4(0.0f, 0.0f, 0.0f, 0.45f));
    m_hud.rect(x, y, 4.0f * u, 112.0f * u, vec4(0.9f, 0.15f, 0.1f, 0.9f));
    m_hud.text(head, x + 16.0f * u, y + 10.0f * u, pxS, kGrey);
    m_hud.text("OBJECTIVE:", x + 16.0f * u, y + 36.0f * u, pxS, kYellow);
    m_hud.textShadow(obj, x + 16.0f * u, y + 58.0f * u, pxM + 0.5f * u, kWhite);
    m_hud.text(hint, x + 16.0f * u, y + 88.0f * u, 1.7f * u, kGrey);

    // zombies and kills, top-right
    char buf[64];
    std::snprintf(buf, sizeof(buf), "ZOMBIES REMAINING: %d", info.zombiesAlive);
    float zw = m_hud.textWidth(buf, 2.5f * u);
    m_hud.rect(w - zw - 40.0f * u, 16.0f * u, zw + 28.0f * u, 34.0f * u, vec4(0.0f, 0.0f, 0.0f, 0.45f));
    m_hud.text(buf, w - zw - 26.0f * u, 24.0f * u, 2.5f * u, kWhite);
    std::snprintf(buf, sizeof(buf), "KILLS: %d", info.kills);
    float kw = m_hud.textWidth(buf, 2.5f * u);
    m_hud.rect(w - kw - 40.0f * u, 56.0f * u, kw + 28.0f * u, 34.0f * u, vec4(0.0f, 0.0f, 0.0f, 0.45f));
    m_hud.text(buf, w - kw - 26.0f * u, 64.0f * u, 2.5f * u, kYellow);

    // ammunition, bottom-right: a row of bullet icons for the magazine, and the numbers
    {
        std::snprintf(buf, sizeof(buf), "AMMO: %d / %d", info.ammoMagazine, info.ammoReserve);
        float aw = m_hud.textWidth(buf, 3.5f * u);
        float ax = w - aw - 30.0f * u, ay = h - 58.0f * u;
        m_hud.rect(ax - 14.0f * u, ay - 12.0f * u, aw + 28.0f * u, 54.0f * u, vec4(0.0f, 0.0f, 0.0f, 0.55f));
        vec4 col = (info.ammoMagazine == 0) ? kRed : (info.ammoMagazine <= 3 ? kYellow : kWhite);
        m_hud.textShadow(buf, ax, ay, 3.5f * u, col);
        float iw = 7.0f * u, ih = 15.0f * u, gap = 3.5f * u;
        float rowW = (float)info.ammoCapacity * (iw + gap) - gap, ix = w - 30.0f * u - rowW + 14.0f * u - 14.0f * u;
        for (int i = 0; i < info.ammoCapacity; ++i)
            m_hud.rect(ix + (float)i * (iw + gap), ay - 34.0f * u, iw, ih,
                       i < info.ammoMagazine ? vec4(0.95f, 0.75f, 0.25f, 0.95f) : vec4(0.25f, 0.25f, 0.28f, 0.7f));
        std::string note;
        if (info.reloading) note = "RELOADING...";
        else if (info.ammoMagazine == 0) note = (info.ammoReserve > 0) ? "EMPTY - PRESS R TO RELOAD" : "OUT OF AMMUNITION";
        if (!note.empty()) {
            float flash = info.reloading ? 1.0f : 0.65f + 0.35f * (float)std::sin(info.time * 8.0);
            m_hud.textShadow(note, w - m_hud.textWidth(note, 2.0f * u) - 30.0f * u, ay - 56.0f * u, 2.0f * u,
                             info.reloading ? vec4(1.0f, 0.85f, 0.25f, 1.0f) : vec4(1.0f, 0.25f, 0.2f, flash));
        }
    }

    // crosshair (tighter, red and with a centre dot while aiming)
    if (info.alive) {
        float cx = w * 0.5f, cy = h * 0.5f;
        float a = info.aimBlend;
        float gap = (10.0f - 6.0f * a) * u, len = (10.0f - 2.0f * a) * u, th = 2.0f * u;
        vec4 c = (a > 0.5f) ? vec4(1.0f, 0.30f, 0.25f, 0.95f) : vec4(1.0f, 1.0f, 1.0f, 0.80f);
        vec4 shadow(0.0f, 0.0f, 0.0f, 0.5f);
        for (int pass = 0; pass < 2; ++pass) {
            float o = (pass == 0) ? 1.0f * u : 0.0f;
            const vec4& col = (pass == 0) ? shadow : c;
            m_hud.rect(cx - gap - len + o, cy - th * 0.5f + o, len, th, col);
            m_hud.rect(cx + gap + o,       cy - th * 0.5f + o, len, th, col);
            m_hud.rect(cx - th * 0.5f + o, cy - gap - len + o, th, len, col);
            m_hud.rect(cx - th * 0.5f + o, cy + gap + o,       th, len, col);
            if (a > 0.5f) m_hud.rect(cx - th * 0.5f + o, cy - th * 0.5f + o, th, th, col);
        }
        if (info.reloading) {
            float bw = 70.0f * u;
            m_hud.rect(cx - bw * 0.5f, cy + 34.0f * u, bw, 6.0f * u, vec4(0.0f, 0.0f, 0.0f, 0.6f));
            m_hud.rect(cx - bw * 0.5f, cy + 34.0f * u, bw * info.reloadProgress, 6.0f * u, kYellow);
        }
        if (info.hitMarker > 0.0f) {
            vec4 hc = info.hitKilled ? vec4(1.0f, 0.15f, 0.1f, info.hitMarker) : vec4(1.0f, 1.0f, 1.0f, info.hitMarker);
            float d = 14.0f * u, s = 6.0f * u;
            m_hud.rect(cx - d - s, cy - d - s, s, s, hc);
            m_hud.rect(cx + d,     cy - d - s, s, s, hc);
            m_hud.rect(cx - d - s, cy + d,     s, s, hc);
            m_hud.rect(cx + d,     cy + d,     s, s, hc);
        }
    }

    // health, bottom-left
    float bx = 24.0f * u, bh = 18.0f * u, bw = 270.0f * u, by = h - 44.0f * u;
    std::snprintf(buf, sizeof(buf), "HEALTH: %d%%", info.healthPercent);
    m_hud.textShadow(buf, bx, by - 26.0f * u, 2.5f * u, kWhite);
    m_hud.rect(bx - 3.0f * u, by - 3.0f * u, bw + 6.0f * u, bh + 6.0f * u, vec4(0.0f, 0.0f, 0.0f, 0.65f));
    float f = saturate(info.healthFraction);
    m_hud.rect(bx, by, bw * f, bh, vec4(1.0f - f, 0.15f + 0.65f * f, 0.10f, 0.95f));

    // objective marker
    if (info.marker.valid && (info.marker.distance > 2.0f || info.marker.offscreen)) {
        float s = 12.0f * u;
        float mx = info.marker.x, my = info.marker.y;
        float pulse = 0.75f + 0.25f * (float)std::sin(info.time * 4.0);
        m_hud.rect(mx - s * 0.5f - 2.0f * u, my - s * 0.5f - 2.0f * u, s + 4.0f * u, s + 4.0f * u, vec4(0.0f, 0.0f, 0.0f, 0.7f));
        m_hud.rect(mx - s * 0.5f, my - s * 0.5f, s, s, vec4(1.0f, 0.85f, 0.2f, pulse));
        std::snprintf(buf, sizeof(buf), "%d M", (int)std::round(info.marker.distance));
        m_hud.textShadow(buf, mx - m_hud.textWidth(buf, 2.0f * u) * 0.5f, my + s * 0.5f + 6.0f * u, 2.0f * u, kYellow);
    }

    // interaction prompt with a key cap
    if (m.hasPrompt()) {
        float pw = m_hud.textWidth(m.prompt(), 3.0f * u) + 44.0f * u;
        float py = h * 0.72f;
        m_hud.rect(w * 0.5f - pw * 0.5f, py - 12.0f * u, pw, 50.0f * u, vec4(0.0f, 0.0f, 0.0f, 0.62f));
        m_hud.rect(w * 0.5f - pw * 0.5f, py - 12.0f * u, 4.0f * u, 50.0f * u, vec4(1.0f, 0.85f, 0.25f, 0.95f));
        m_hud.textCentered(m.prompt(), w * 0.5f, py, 3.0f * u, kYellow);
    }

    // mission message (fades in and out), upper middle
    if (m.hasMessage()) m_hud.textCentered(m.messageText(), w * 0.5f, h * 0.22f, 4.0f * u, vec4(1.0f, 1.0f, 1.0f, m.messageAlpha()));

    // "OBJECTIVE COMPLETE" banner
    if (info.toastAlpha > 0.0f && !info.toast.empty()) {
        float tw = m_hud.textWidth(info.toast, 3.0f * u) + 60.0f * u, ty = h * 0.13f;
        m_hud.rect(w * 0.5f - tw * 0.5f, ty - 12.0f * u, tw, 46.0f * u, vec4(0.0f, 0.15f, 0.05f, 0.7f * info.toastAlpha));
        m_hud.rect(w * 0.5f - tw * 0.5f, ty - 12.0f * u, 4.0f * u, 46.0f * u, vec4(0.25f, 0.95f, 0.45f, info.toastAlpha));
        m_hud.textCentered(info.toast, w * 0.5f, ty, 3.0f * u, vec4(0.25f, 0.95f, 0.45f, info.toastAlpha));
    }

    // debug banner
    if (info.debugMode) {
        std::string d = std::string("DEBUG MODE (F1)") + (info.godMode ? "  GOD MODE" : "") + "   K KILL  G GOD  U CARD  1-6 TELEPORT";
        m_hud.textCentered(d, w * 0.5f, h - 24.0f * u, 1.8f * u, vec4(1.0f, 0.3f, 0.2f, 0.95f));
    }

    // the power flicker at the very start of the mission
    float fl = m.flickerOverlay();
    if (fl > 0.0f) m_hud.fullscreen(vec4(0.0f, 0.0f, 0.0f, fl));
}

void GameUi::paused(int selected) const
{
    float u = ui(), w = (float)m_hud.width(), h = (float)m_hud.height();
    m_hud.fullscreen(vec4(0.0f, 0.0f, 0.0f, 0.62f));
    vignette(1.2f);
    m_hud.textCentered("PAUSED", w * 0.5f, h * 0.14f, 7.0f * u, kWhite);
    menuList(pauseMenuItems(), selected, h * 0.34f, 3.4f * u);
    m_hud.textCentered("ESC: RESUME     W / S: CHOOSE     ENTER: CONFIRM", w * 0.5f, h - 40.0f * u, 2.0f * u, kGrey);
}

void GameUi::failed(float t) const
{
    float u = ui(), w = (float)m_hud.width(), h = (float)m_hud.height();
    float a = reveal(t, 0.0f, 1.2f);
    m_hud.fullscreen(vec4(0.15f, 0.0f, 0.0f, 0.70f * a));
    vignette(1.8f * a);
    m_hud.textCentered("MISSION FAILED", w * 0.5f, h * 0.30f, 9.0f * u, withAlpha(kRed, a));
    m_hud.textCentered("YOU DID NOT MAKE IT OUT OF BARIDHARA.", w * 0.5f, h * 0.30f + 100.0f * u, 3.0f * u, withAlpha(kGrey, reveal(t, 0.8f)));
    float b = reveal(t, 1.6f);
    m_hud.textCentered("PRESS R TO RESTART THE MISSION", w * 0.5f, h * 0.62f, 3.5f * u, withAlpha(kWhite, b));
    m_hud.textCentered("PRESS M FOR THE MAIN MENU", w * 0.5f, h * 0.62f + 44.0f * u, 2.5f * u, withAlpha(kGrey, b));
}

void GameUi::completed(const MissionManager& m, float t) const
{
    float u = ui(), w = (float)m_hud.width(), h = (float)m_hud.height();
    m_hud.fullscreen(vec4(0.0f, 0.0f, 0.0f, 0.78f * reveal(t, 0.0f, 1.0f)));
    vignette(1.2f);
    m_hud.textCentered("DHAKA: THE LAST SURVIVORS", w * 0.5f, h * 0.10f, 5.0f * u, withAlpha(kWhite, reveal(t, 0.4f)));
    m_hud.textCentered("THE FALL OF BARIDHARA", w * 0.5f, h * 0.10f + 54.0f * u, 3.0f * u, withAlpha(kGrey, reveal(t, 0.6f)));
    m_hud.textCentered("MISSION COMPLETED", w * 0.5f, h * 0.27f, 8.0f * u, withAlpha(kGreen, reveal(t, 1.2f)));
    m_hud.textCentered("YOU ESCAPED THE UNIVERSITY. BUT DHAKA IS STILL LOST...", w * 0.5f, h * 0.27f + 90.0f * u, 3.0f * u, withAlpha(kWhite, reveal(t, 2.2f)));

    int secs = (int)m.playTime();
    char buf[96];
    float px = 3.0f * u, y = h * 0.55f;
    std::snprintf(buf, sizeof(buf), "TIME TAKEN: %02d:%02d", secs / 60, secs % 60);
    m_hud.textCentered(buf, w * 0.5f, y, px, withAlpha(kYellow, reveal(t, 3.2f)));
    std::snprintf(buf, sizeof(buf), "ZOMBIES KILLED: %d", m.zombiesKilled());
    m_hud.textCentered(buf, w * 0.5f, y + 38.0f * u, px, withAlpha(kYellow, reveal(t, 3.5f)));
    std::snprintf(buf, sizeof(buf), "HEALTH REMAINING: %d%%", m.finalHealthPercent());
    m_hud.textCentered(buf, w * 0.5f, y + 76.0f * u, px, withAlpha(kYellow, reveal(t, 3.8f)));
    std::snprintf(buf, sizeof(buf), "OBJECTIVES COMPLETED: %d/%d", m.objectivesDone(), (int)StageCount);
    m_hud.textCentered(buf, w * 0.5f, y + 114.0f * u, px, withAlpha(kYellow, reveal(t, 4.1f)));

    m_hud.textCentered("R: PLAY AGAIN      M: MAIN MENU", w * 0.5f, h - 56.0f * u, 2.5f * u, withAlpha(kGrey, reveal(t, 5.0f)));
}
