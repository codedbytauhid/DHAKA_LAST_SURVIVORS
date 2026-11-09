// src/main.cpp
// Milestone 7: audio, particles, atmosphere, settings and polish on top of the finished game.
//
// Normal controls
//   W A S D move | Mouse look | Shift sprint | Space jump | E interact | Wheel camera distance
//   Right mouse AIM | Left mouse SHOOT | R reload | Tab free the mouse | Esc pause
// Debug mode (press F1 while playing): K kill nearest zombie, G god mode, U give access card, 1..6 teleport
// (none of the debug keys do anything outside debug mode)
// Command line:  main.exe --export-audio   writes every built-in sound as a WAV file into assets/audio/ and quits
#include "glad.h"   // must come BEFORE glfw3.h
#include "glfw3.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render/Shader.h"
#include "render/Camera.h"
#include "world/Level.h"
#include "game/Player.h"
#include "game/ZombieManager.h"
#include "game/Mission.h"
#include "game/Weapon.h"
#include "game/Combat.h"
#include "game/ShotEffects.h"
#include "game/Settings.h"
#include "audio/AudioManager.h"
#include "audio/GameAudio.h"
#include "fx/ParticleSystem.h"
#include "ui/Hud.h"
#include "ui/GameUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static const int kZombieCount = 12;                 // 1..12 (8 is lighter)
static const char* kSettingsFile = "settings.cfg";  // saved next to the game
static const char* kAudioDir = "assets/audio";      // optional WAV files that replace built-in sounds

static double g_scrollY = 0.0;                      // mouse wheel movement collected by the callback

static void framebufferSizeCallback(GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); }
static void scrollCallback(GLFWwindow*, double, double y) { g_scrollY += y; }

// Remembers which keys / buttons were down last frame, so we can detect "pressed THIS frame".
struct Input {
    bool cur[GLFW_KEY_LAST + 1] = {};
    bool prev[GLFW_KEY_LAST + 1] = {};
    bool mcur[3] = {}, mprev[3] = {};
    void poll(GLFWwindow* w)
    {
        for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k) { prev[k] = cur[k]; cur[k] = glfwGetKey(w, k) == GLFW_PRESS; }
        for (int b = 0; b < 3; ++b) { mprev[b] = mcur[b]; mcur[b] = glfwGetMouseButton(w, b) == GLFW_PRESS; }
    }
    bool down(int k) const { return cur[k]; }
    bool pressed(int k) const { return cur[k] && !prev[k]; }
    bool mouseDown(int b) const { return mcur[b]; }
    bool mousePressed(int b) const { return mcur[b] && !mprev[b]; }
    bool confirm() const { return pressed(GLFW_KEY_ENTER) || pressed(GLFW_KEY_KP_ENTER) || pressed(GLFW_KEY_SPACE); }
    bool up() const { return pressed(GLFW_KEY_W) || pressed(GLFW_KEY_UP); }
    bool downKey() const { return pressed(GLFW_KEY_S) || pressed(GLFW_KEY_DOWN); }
    bool leftHeld() const { return down(GLFW_KEY_A) || down(GLFW_KEY_LEFT); }
    bool rightHeld() const { return down(GLFW_KEY_D) || down(GLFW_KEY_RIGHT); }
    bool leftOrRightPressed() const { return pressed(GLFW_KEY_A) || pressed(GLFW_KEY_LEFT) || pressed(GLFW_KEY_D) || pressed(GLFW_KEY_RIGHT); }
};

enum class Overlay { None, Controls, Settings };      // screens drawn over the main menu or the pause menu

// Where does a 3D point appear on screen? Used for the yellow objective marker.
static MarkerInfo projectMarker(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& target,
                                const glm::vec3& playerPos, int w, int h)
{
    MarkerInfo m;
    glm::vec4 c = proj * view * glm::vec4(target, 1.0f);
    float nx, ny;
    bool behind = c.w <= 0.001f;
    if (behind) {
        nx = -c.x; ny = -c.y;
        float mx = std::max(std::fabs(nx), std::fabs(ny));
        if (mx < 0.0001f) { nx = 0.0f; ny = -1.0f; mx = 1.0f; }
        nx /= mx; ny /= mx;
    } else {
        nx = c.x / c.w; ny = c.y / c.w;
    }
    m.offscreen = behind || std::fabs(nx) > 0.9f || std::fabs(ny) > 0.85f;
    nx = std::max(-0.9f, std::min(0.9f, nx));
    ny = std::max(-0.85f, std::min(0.85f, ny));
    m.x = (nx * 0.5f + 0.5f) * (float)w;
    m.y = (1.0f - (ny * 0.5f + 0.5f)) * (float)h;
    m.distance = glm::length(target - playerPos);
    m.valid = true;
    return m;
}

static float saturate(float v) { return std::max(0.0f, std::min(1.0f, v)); }
static glm::vec3 mixv(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * t; }

int main(int argc, char** argv)
{
    // ---- optional tool: write the built-in sounds to disk, so you can listen to them or replace them
    if (argc > 1 && std::strcmp(argv[1], "--export-audio") == 0) {
        AudioManager a;
        a.init(false, kAudioDir);
        std::printf("Wrote %d WAV files to %s/ (the folder must already exist)\n", a.exportWavs(kAudioDir), kAudioDir);
        return 0;
    }

    if (!glfwInit()) { std::fprintf(stderr, "ERROR: glfwInit failed\n"); return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "DHAKA: THE LAST SURVIVORS", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "ERROR: no OpenGL 3.3 Core window.\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "ERROR: GLAD failed\n");
        glfwTerminate();
        return -1;
    }
    std::printf("GL Renderer : %s\n", (const char*)glGetString(GL_RENDERER));
    std::printf("GL Version  : %s\n", (const char*)glGetString(GL_VERSION));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Everything that owns OpenGL objects (or the sound device) lives in this block so it is destroyed
    // BEFORE glfwTerminate() destroys the OpenGL context.
    {
        Shader shader("shaders/lit.vert", "shaders/lit.frag");
        if (!shader.isValid()) {
            std::fprintf(stderr, "Shader failed. See the message above.\n");
            glfwTerminate();
            return -1;
        }

        Level level;
        Player player;
        Camera camera;
        ZombieManager zombies;
        Hud hud;
        if (!hud.isValid()) {
            std::fprintf(stderr, "HUD shader failed. See the message above.\n");
            glfwTerminate();
            return -1;
        }
        GameUi ui(hud);
        MissionManager mission;
        Weapon weapon;
        ShotEffects fx;
        ParticleSystem particles;
        if (!particles.isValid()) std::fprintf(stderr, "Particle shader failed: particles are disabled.\n");
        float hitMarker = 0.0f;                // fades after a bullet hits a zombie
        bool hitKilled = false;

        // ---- settings and sound
        Settings settings;
        settings.load(kSettingsFile);
        settings.clampValues();
        std::printf("[Audio] generating sounds... ");
        std::fflush(stdout);
        AudioManager audio;
        audio.init(true, kAudioDir);
        std::printf("done.%s\n", audio.deviceOpen() ? "" : " (no sound device: running silently)");
        GameAudio ga(audio);
        auto applySettings = [&]() {
            audio.setVolumes(settings.masterVolume, settings.musicVolume, settings.sfxVolume, settings.ambienceVolume);
            camera.sensitivity = settings.mouseSensitivity;
        };
        applySettings();

        // a wall between a sound and the listener makes it quieter and duller
        ga.isBlocked = [&](const glm::vec3& from, const glm::vec3& to) {
            glm::vec3 d = to - from;
            float len = glm::length(d);
            if (len < 0.5f) return false;
            return level.collision().raycast(from, d / len, len) < len - 0.5f;
        };

        zombies.spawn(kZombieCount, level.collision());
        zombies.onEvent = [&](ZombieEvent e, const glm::vec3& at) { ga.zombieEvent(e, at); };   // zombie sounds, positioned in the world

        std::printf("Level: %zu props, %zu point lights, %zu collision boxes.\n",
                    level.propCount(), level.lightCount(), level.collision().count());

        // ---- the look: sunset sky, indoor fog and how it changes
        const glm::vec3 fogOut(0.72f, 0.50f, 0.42f), fogIn(0.16f, 0.14f, 0.15f);
        const glm::vec3 sunLate(0.95f, 0.36f, 0.22f), sunEarly(1.0f, 0.62f, 0.38f);
        const glm::vec3 ambEarly(0.24f, 0.21f, 0.29f), ambLate(0.19f, 0.16f, 0.27f);
        float fogBlend = 0.0f;

        // Debug teleport spots (keys 1..6 in debug mode) - the first one is also where the mission starts.
        struct Spot { glm::vec3 feet; float dirX, dirZ; };
        const Spot spots[6] = {
            { { 0.0f, 0.05f,  3.2f},  0.0f, -1.0f },   // 1 classroom, facing the blackboard
            { {-9.5f, 0.05f,  5.7f},  1.0f,  0.0f },   // 2 corridor
            { {-11.0f, 0.05f, 5.0f}, -1.0f,  0.0f },   // 3 staircase
            { { 3.6f, 0.05f, 12.0f},  0.0f,  1.0f },   // 4 courtyard
            { { 3.6f, 0.05f, 30.0f},  0.0f,  1.0f },   // 5 near the gate
            { { 3.6f, 0.05f, 39.0f},  0.0f, -1.0f },   // 6 road
        };
        auto teleportTo = [&](int i) {
            const Spot& s = spots[i];
            player.teleport(s.feet, std::atan2(s.dirX, s.dirZ));
            camera.yaw = glm::degrees(std::atan2(s.dirZ, s.dirX));
            camera.pitch = -12.0f;
            camera.snapTo(s.feet);
        };
        // Puts EVERYTHING back to the way it was at the start of the mission.
        bool godMode = false;
        auto resetWorld = [&]() {
            player.revive();
            zombies.reset();
            level.resetMission();
            weapon.reset();                  // full starting ammunition (12 + 36)
            fx.clear();
            particles.clear();
            level.setMuzzleLight(glm::vec3(0.0f), 0.0f);
            hitMarker = 0.0f;
            godMode = false;
            teleportTo(0);
        };
        resetWorld();

        bool debugMode = false;
        int menuIndex = 0, pauseIndex = 0, settingsRow = 0;
        Overlay overlay = Overlay::None;
        bool tabFree = false, captured = false, firstMouse = true;
        double lastMouseX = 0.0, lastMouseY = 0.0;
        double lastTime = glfwGetTime(), lastFpsTime = lastTime, stateEnterTime = lastTime;
        int frames = 0;
        Input input;
        float settingsRepeat = 0.0f;
        std::string toast; float toastT = 0.0f;                     // "OBJECTIVE COMPLETE" banner
        float dustAcc = 0.0f, smokeAcc[2] = { 0.0f, 0.0f };         // particle emitters' timers
        GameState prevState = GameState::MainMenu;
        ga.stateChanged(GameState::MainMenu, GameState::MainMenu);  // starts the menu music

        const glm::vec3 smokeSource[2] = { glm::vec3(-11.5f, 2.6f, 38.1f), glm::vec3(34.0f, 14.0f, 56.0f) };   // a burning bus, a burning flat

        while (!glfwWindowShouldClose(window)) {
            double now = glfwGetTime();
            float dt = (float)(now - lastTime);
            lastTime = now;
            if (dt > 0.1f) dt = 0.1f;
            input.poll(window);

            // ================= what the mission needs to know this frame =================
            MissionFrame mf;
            mf.dt = dt;
            mf.playerPos = player.position();
            mf.playerAlive = !player.dead();
            mf.zombiesAlive = zombies.alive();
            mf.zombiesTotal = zombies.total();
            mf.playerHealth = player.health();
            mf.world = &level.collision();

            GameState state = mission.state();

            // ================= input, by state =================
            bool overlayActive = (overlay != Overlay::None) && (state == GameState::MainMenu || state == GameState::Paused);
            if (overlayActive) {
                if (overlay == Overlay::Controls) {
                    if (input.pressed(GLFW_KEY_ESCAPE) || input.confirm()) { overlay = Overlay::None; ga.uiSelect(); }
                } else {                                                       // settings: sliders applied and saved immediately
                    if (input.up()) { settingsRow = (settingsRow + GameUi::kSettingsRows - 1) % GameUi::kSettingsRows; ga.uiSelect(); }
                    if (input.downKey()) { settingsRow = (settingsRow + 1) % GameUi::kSettingsRows; ga.uiSelect(); }
                    int dir = (input.rightHeld() ? 1 : 0) - (input.leftHeld() ? 1 : 0);
                    settingsRepeat -= dt;
                    if (dir != 0 && (input.leftOrRightPressed() || settingsRepeat <= 0.0f)) {
                        settingsRepeat = input.leftOrRightPressed() ? 0.35f : 0.07f;      // hold the key to keep sliding
                        float* values[4] = { &settings.masterVolume, &settings.musicVolume, &settings.sfxVolume, &settings.ambienceVolume };
                        if (settingsRow < 4) *values[settingsRow] += 0.05f * (float)dir;
                        else settings.mouseSensitivity += 0.01f * (float)dir;
                        settings.clampValues();
                        applySettings();
                        if (settingsRow == 2 || settingsRow == 0) ga.uiSelect();           // an audible preview
                    }
                    if (dir == 0) settingsRepeat = 0.0f;
                    if (input.pressed(GLFW_KEY_ESCAPE) || input.confirm()) { overlay = Overlay::None; settings.save(kSettingsFile); ga.uiConfirm(); }
                }
            } else switch (state) {
            case GameState::MainMenu:
                if (input.up()) { menuIndex = (menuIndex + 3) % 4; ga.uiSelect(); }
                if (input.downKey()) { menuIndex = (menuIndex + 1) % 4; ga.uiSelect(); }
                if (input.confirm()) {
                    ga.uiConfirm();
                    if (menuIndex == 0) { resetWorld(); mission.startMission(); }
                    else if (menuIndex == 1) overlay = Overlay::Controls;
                    else if (menuIndex == 2) { overlay = Overlay::Settings; settingsRow = 0; }
                    else glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                if (input.pressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;

            case GameState::Intro:
                if (input.confirm() || input.pressed(GLFW_KEY_ESCAPE) || input.pressed(GLFW_KEY_E)) mission.skipIntro();
                break;

            case GameState::Playing:
                if (input.pressed(GLFW_KEY_ESCAPE)) { mission.pause(); pauseIndex = 0; break; }
                if (input.pressed(GLFW_KEY_TAB)) tabFree = !tabFree;
                if (input.pressed(GLFW_KEY_E)) {
                    mission.interact(mf);
                    int rounds = mission.takeAmmoReward();           // an ammunition crate was collected
                    if (rounds > 0) weapon.addReserve(rounds);
                }
                if (input.pressed(GLFW_KEY_LEFT_BRACKET))  { settings.mouseSensitivity -= 0.02f; settings.clampValues(); applySettings(); }
                if (input.pressed(GLFW_KEY_RIGHT_BRACKET)) { settings.mouseSensitivity += 0.02f; settings.clampValues(); applySettings(); }
                camera.distance -= (float)g_scrollY * 0.4f;
                camera.distance = std::max(camera.minDistance, std::min(camera.maxDistance, camera.distance));

                if (input.pressed(GLFW_KEY_F1)) { debugMode = !debugMode; std::printf("[Debug] debug mode %s\n", debugMode ? "ON" : "OFF"); }
                if (debugMode) {
                    for (int i = 0; i < 6; ++i) if (input.pressed(GLFW_KEY_1 + i)) teleportTo(i);
                    if (input.pressed(GLFW_KEY_K) && !zombies.killNearest(player.position(), 6.0f))
                        std::printf("[Debug] no living zombie within 6 m\n");
                    if (input.pressed(GLFW_KEY_G)) { godMode = !godMode; std::printf("[Debug] god mode %s\n", godMode ? "ON" : "OFF"); }
                    if (input.pressed(GLFW_KEY_U)) mission.debugGiveCard(mf);
                }
                break;

            case GameState::Paused:
                if (input.pressed(GLFW_KEY_ESCAPE)) { mission.resume(); break; }
                if (input.pressed(GLFW_KEY_R)) { resetWorld(); mission.restart(); break; }
                if (input.up()) { pauseIndex = (pauseIndex + 5) % 6; ga.uiSelect(); }
                if (input.downKey()) { pauseIndex = (pauseIndex + 1) % 6; ga.uiSelect(); }
                if (input.confirm()) {
                    ga.uiConfirm();
                    if (pauseIndex == 0) mission.resume();
                    else if (pauseIndex == 1) overlay = Overlay::Controls;                 // controls are reachable from the pause menu too
                    else if (pauseIndex == 2) { overlay = Overlay::Settings; settingsRow = 0; }
                    else if (pauseIndex == 3) { resetWorld(); mission.restart(); }
                    else if (pauseIndex == 4) { resetWorld(); mission.goToMainMenu(); menuIndex = 0; }
                    else glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                break;

            case GameState::MissionFailed:
            case GameState::MissionCompleted:
                if (input.pressed(GLFW_KEY_R)) { resetWorld(); mission.restart(); }
                else if (input.pressed(GLFW_KEY_M)) { resetWorld(); mission.goToMainMenu(); menuIndex = 0; }
                break;
            }
            g_scrollY = 0.0;
            state = mission.state();                   // may have changed above

            // a new screen: tell the audio (music, ambience, pause) and remember when it started (for fade-ins)
            if (state != prevState) {
                ga.stateChanged(prevState, state);
                stateEnterTime = now;
                if (state != GameState::MainMenu && state != GameState::Paused) overlay = Overlay::None;
                if (state == GameState::Playing && prevState != GameState::Paused) toastT = 0.0f;
                prevState = state;
            }

            // mouse capture follows the state (only while playing do we hide the cursor)
            bool wantCapture = (state == GameState::Playing && !tabFree);
            if (wantCapture != captured) {
                captured = wantCapture;
                glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                firstMouse = true;
            }
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            if (firstMouse) { lastMouseX = mx; lastMouseY = my; firstMouse = false; }
            if (captured) camera.rotate((float)(mx - lastMouseX), (float)(my - lastMouseY));
            lastMouseX = mx; lastMouseY = my;

            // ================= update, by state =================
            PlayerInput in;
            if (state == GameState::Playing) {
                if (input.down(GLFW_KEY_W)) in.forward += 1.0f;
                if (input.down(GLFW_KEY_S)) in.forward -= 1.0f;
                if (input.down(GLFW_KEY_D)) in.right   += 1.0f;
                if (input.down(GLFW_KEY_A)) in.right   -= 1.0f;
                in.sprint = input.down(GLFW_KEY_LEFT_SHIFT) || input.down(GLFW_KEY_RIGHT_SHIFT);
                in.jump   = input.down(GLFW_KEY_SPACE);
            }
            glm::vec3 camF = camera.front(); camF.y = 0.0f; camF = glm::normalize(camF);
            glm::vec3 camR = camera.right(); camR.y = 0.0f; camR = glm::normalize(camR);

            // ---- weapon handling (only while playing with the mouse captured and the player alive)
            bool canAct = (state == GameState::Playing) && captured && !player.dead();
            bool aimHeld = canAct && input.mouseDown(GLFW_MOUSE_BUTTON_RIGHT);
            if (state == GameState::Playing) {
                weapon.update(dt, aimHeld);
                if (canAct && input.pressed(GLFW_KEY_R) && weapon.startReload()) ga.pistolReload();
                hitMarker = std::max(0.0f, hitMarker - dt * 4.0f);
                in.aiming = aimHeld;
                in.faceCamera = aimHeld || weapon.timeSinceShot() < 0.6f;
                in.camYaw = std::atan2(camF.x, camF.z);
                in.aimPitch = std::asin(std::max(-0.7f, std::min(0.7f, camera.front().y)));
            }

            switch (state) {
            case GameState::MainMenu: {
                camera.position = glm::vec3(3.6f + 2.0f * (float)std::sin(now * 0.12), 2.4f, 13.0f);
                camera.yaw = 90.0f + 28.0f * (float)std::sin(now * 0.09);
                camera.pitch = -6.0f;
                zombies.update(dt, glm::vec3(0.0f, 0.0f, -200.0f), 0.0f, false, level.collision());
                break;
            }
            case GameState::Intro:
                mission.update(mf);
                break;
            case GameState::Playing: {
                player.update(dt, in, camF, camR, level.collision());

                // footsteps, jumps and landings
                PlayerEvents pe = player.takeEvents();
                float sunHere, ambHere;
                level.lightingAt(player.position() + glm::vec3(0.0f, 0.9f, 0.0f), sunHere, ambHere);
                bool indoorsNow = sunHere < 0.9f;
                if (pe.step) ga.footstep(pe.run, indoorsNow);
                if (pe.jumped) ga.jump();
                if (pe.landed) ga.land();

                float damage = zombies.update(dt, player.position(), player.speed(), !player.dead(), level.collision());
                if (damage > 0.0f && !godMode && !player.dead()) {
                    player.takeDamage(damage);
                    if (player.dead()) ga.playerDeath(); else ga.playerHurt();
                }

                camera.aimBlend = weapon.aimBlend();
                camera.update(player.position(), dt, level.collision());

                // ---- SHOOTING (left mouse button, one shot per click). See Combat.cpp for how the hit is found.
                if (canAct && input.mousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
                    Weapon::Fire r = weapon.tryFire();
                    if (r == Weapon::Fire::Fired) {
                        glm::vec3 front = camera.front();
                        glm::vec3 chest = player.chestPosition();
                        float along = std::max(0.0f, glm::dot(chest - camera.position, front));
                        glm::vec3 camOrigin = camera.position + front * along;
                        ShotResult shot = resolveShot(camOrigin, front, chest, weapon.cfg.range, weapon.cfg.damage,
                                                      weapon.cfg.headshotMultiplier, level.collision(), zombies.zombies());
                        glm::vec3 muzzle = player.muzzlePosition();
                        fx.addShot(muzzle, shot.impact, shot.hitZombie);
                        zombies.alertNoise(player.position(), 10.0f);
                        ga.pistolShot();
                        particles.pool().muzzleFlash(muzzle, shot.dir);
                        particles.pool().muzzleSmoke(muzzle);
                        if (shot.hitZombie) {
                            hitMarker = 1.0f;
                            hitKilled = shot.killed;
                            particles.pool().blood(shot.impact, shot.dir, shot.killed);
                            if (shot.killed) zombies.announceDeath(shot.impact); else zombies.announceHit(shot.impact);
                        } else if (shot.hitWorld) {
                            particles.pool().wallImpact(shot.impact, shot.dir);
                            ga.bulletImpact(shot.impact);
                        }
                    } else if (r == Weapon::Fire::MagazineEmpty) {
                        ga.pistolEmpty();
                        mission.pushMessage("MAGAZINE EMPTY. PRESS R TO RELOAD.", 1.6f);
                    } else if (r == Weapon::Fire::OutOfAmmo) {
                        ga.pistolEmpty();
                        mission.pushMessage("OUT OF AMMUNITION. FIND MORE.", 1.8f);
                    }
                }
                fx.update(dt);
                level.setMuzzleLight(player.muzzlePosition(), weapon.muzzleFlash());       // the flash lights up the surroundings

                mf.playerPos = player.position();
                mf.playerAlive = !player.dead();
                mf.zombiesAlive = zombies.alive();
                mf.playerHealth = player.health();
                mission.update(mf);
                break;
            }
            case GameState::MissionFailed:
                camera.aimBlend = 0.0f;
                player.update(dt, in, camF, camR, level.collision());
                zombies.update(dt, player.position(), 0.0f, false, level.collision());
                camera.update(player.position(), dt, level.collision());
                level.setMuzzleLight(glm::vec3(0.0f), 0.0f);
                break;
            case GameState::MissionCompleted:
                camera.aimBlend = 0.0f;
                player.update(dt, in, camF, camR, level.collision());
                camera.update(player.position(), dt, level.collision());
                level.setMuzzleLight(glm::vec3(0.0f), 0.0f);
                break;
            case GameState::Paused:
                break;
            }

            // mission events -> sounds and the "OBJECTIVE COMPLETE" banner
            MissionEvent me;
            while (mission.pollEvent(me)) {
                ga.missionEvent(me);
                if (me == MissionEvent::ObjectiveComplete) { toast = "OBJECTIVE COMPLETE"; toastT = 2.6f; }
            }
            toastT = std::max(0.0f, toastT - dt);

            // mission props follow the mission data (cheap when nothing changed)
            level.setCardVisible(!mission.hasCard());
            level.setPanelUnlocked(mission.gateUnlocked());
            level.setGateProgress(mission.gateProgress());
            level.setCheckpointActive(mission.checkpointActive());
            for (int i = 0; i < MissionManager::kAmmoSites; ++i) level.setAmmoVisible(i, !mission.ammoTaken(i));

            state = mission.state();
            if (player.hurtFlash() > 0.0f && state == GameState::Playing) {
                float s = player.hurtFlash() * player.hurtFlash() * 0.07f;
                camera.position += camera.right() * (float)std::sin(now * 70.0) * s +
                                   glm::vec3(0.0f, 1.0f, 0.0f) * (float)std::cos(now * 61.0) * s;
            }

            // ================= sound: listener, ambience, flickering lights =================
            {
                glm::vec3 listener = (state == GameState::MainMenu || state == GameState::Intro) ? camera.position
                                                                                                  : player.position() + glm::vec3(0.0f, 1.6f, 0.0f);
                float sunL, ambL;
                level.lightingAt(listener, sunL, ambL);
                static std::vector<FlickerProbe> probes;
                probes.clear();
                for (const PointLight& l : level.lights())
                    if (l.flicker == 2) probes.push_back({ l.pos, l.intensity });
                ga.update(dt, state, listener, camera.right(), sunL < 0.9f, mission.introTime(), probes);
            }

            // ================= particles: effects, dust, smoke =================
            if (state != GameState::Paused && state != GameState::Intro) {
                particles.update(dt);
                glm::vec3 cp = camera.position;
                float sunC, ambC;
                level.lightingAt(cp, sunC, ambC);
                bool indoorCam = sunC < 0.9f;
                dustAcc += dt * (indoorCam ? 5.0f : 3.0f);
                while (dustAcc >= 1.0f) {                                   // floating dust specks around the camera
                    dustAcc -= 1.0f;
                    ParticlePool& pp = particles.pool();
                    float r = indoorCam ? 6.0f : 14.0f;
                    particles.pool().dustMote(cp + glm::vec3(pp.randRange(-r, r), pp.randRange(-1.2f, 1.6f), pp.randRange(-r, r)), indoorCam);
                }
                for (int i = 0; i < 2; ++i) {                               // two smoke columns beyond the walls
                    smokeAcc[i] += dt * 5.0f;
                    while (smokeAcc[i] >= 1.0f) { smokeAcc[i] -= 1.0f; particles.pool().smokePuff(smokeSource[i], 0.45f); }
                }
            }

            // ================= atmosphere =================
            float progress = (state == GameState::Playing || state == GameState::Paused) ? saturate(mission.playTime() / 600.0f) : 0.0f;   // the sun slowly sets
            glm::vec3 sunColor = mixv(sunEarly, sunLate, progress);
            glm::vec3 sunDir = glm::normalize(glm::vec3(0.5f, 0.30f - 0.14f * progress, 0.7f));
            glm::vec3 ambient = mixv(ambEarly, ambLate, progress);
            {
                float sunC, ambC;
                level.lightingAt(camera.position, sunC, ambC);
                fogBlend += ((sunC < 0.9f ? 1.0f : 0.0f) - fogBlend) * (1.0f - std::exp(-dt * 2.0f));   // indoors the air is darker and thicker
            }
            glm::vec3 fogColor = mixv(fogOut, fogIn, fogBlend);
            float fogDensity = 0.020f + 0.012f * fogBlend;

            // ================= render =================
            int fbw = 0, fbh = 0;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            if (fbh == 0) { glfwPollEvents(); continue; }   // minimized

            glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 view = camera.view();
            float fov = 60.0f - 16.0f * camera.aimBlend;                       // zoom in a little while aiming
            glm::mat4 projection = glm::perspective(glm::radians(fov), (float)fbw / (float)fbh, 0.1f, 200.0f);

            if (state != GameState::Intro) {
                shader.use();
                shader.setMat4("uView", view);
                shader.setMat4("uProjection", projection);
                shader.setVec3("uViewPos", camera.position);
                shader.setInt("uTexture", 0);
                shader.setVec3("uSunDir", sunDir);
                shader.setVec3("uSunColor", sunColor);
                shader.setVec3("uAmbient", ambient);
                shader.setVec3("uFogColor", fogColor);
                shader.setFloat("uFogDensity", fogDensity);

                level.drawOpaque(shader, camera.position, (float)now);
                if (state != GameState::MainMenu) {
                    float sunScale, ambScale;
                    level.lightingAt(player.position() + glm::vec3(0.0f, 0.9f, 0.0f), sunScale, ambScale);
                    player.draw(shader, sunScale, ambScale, weapon.pose());
                }
                zombies.draw(shader, level, camera.position);
                fx.draw(shader);
                level.drawGlass(shader, camera.position);

                if (particles.isValid()) {
                    float sunC, ambC;
                    level.lightingAt(camera.position, sunC, ambC);
                    particles.draw(view, projection, camera.position, std::max(0.35f, sunC));
                }
            }

            // ---- 2D screens on top
            hud.begin(fbw, fbh);
            float stateTime = (float)(now - stateEnterTime);
            bool menuOverlay = (state == GameState::MainMenu || state == GameState::Paused) && overlay != Overlay::None;
            if (menuOverlay) {
                if (overlay == Overlay::Controls) ui.controls(); else ui.settings(settingsRow, settings);
            } else switch (state) {
            case GameState::MainMenu:
                ui.mainMenu(menuIndex, (float)now);
                break;
            case GameState::Intro:
                ui.intro(mission);
                break;
            case GameState::Playing: {
                PlayingInfo info;
                info.healthFraction = player.health() / player.maxHealth;
                info.healthPercent = (int)std::ceil(player.health());
                info.zombiesAlive = zombies.alive();
                info.hurtFlash = player.hurtFlash();
                info.debugMode = debugMode;
                info.godMode = godMode;
                info.time = now;
                info.ammoMagazine = weapon.magazine();
                info.ammoCapacity = weapon.cfg.magazineSize;
                info.ammoReserve = weapon.reserve();
                info.reloading = weapon.reloading();
                info.reloadProgress = weapon.reloadProgress();
                info.aimBlend = weapon.aimBlend();
                info.kills = zombies.total() - zombies.alive();
                info.hitMarker = hitMarker;
                info.hitKilled = hitKilled;
                info.alive = !player.dead();
                info.toast = toast;
                info.toastAlpha = (toastT > 0.0f) ? saturate(std::min((2.6f - toastT) / 0.3f, toastT / 0.5f)) : 0.0f;
                info.marker = projectMarker(view, projection, mission.markerTarget(), player.position(), fbw, fbh);
                ui.playing(mission, info);
                break;
            }
            case GameState::Paused:
                ui.paused(pauseIndex);
                break;
            case GameState::MissionFailed:
                ui.failed(stateTime);
                break;
            case GameState::MissionCompleted:
                ui.completed(mission, stateTime);
                break;
            }
            hud.end();

            glfwSwapBuffers(window);
            glfwPollEvents();

            ++frames;
            if (now - lastFpsTime >= 1.0) {
                static const char* names[] = { "MENU", "INTRO", "PLAYING", "PAUSED", "FAILED", "COMPLETED" };
                char title[256];
                std::snprintf(title, sizeof(title),
                              "DHAKA: THE LAST SURVIVORS | %s | FPS %d | HP %d | zombies %d/%d | stage %d/6 | particles %zu | voices %d%s%s",
                              names[(int)state], frames, (int)std::ceil(player.health()), zombies.alive(), zombies.total(),
                              mission.objectivesDone(), particles.pool().count(), audio.activeVoices(),
                              debugMode ? " | DEBUG" : "", godMode ? " GOD" : "");
                glfwSetWindowTitle(window, title);
                frames = 0;
                lastFpsTime = now;
            }
        }

        settings.save(kSettingsFile);               // remember the volumes and sensitivity for next time
    }

    glfwTerminate();
    return 0;
}
