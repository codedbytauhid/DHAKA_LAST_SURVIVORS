// src/game/Weapon.h
// The pistol's RULES: ammunition, reloading, fire rate, aiming blend, recoil and muzzle flash timers.
// No drawing and no OpenGL here: main.cpp asks this class "may I fire?", and the Player class asks it for the
// WeaponPose numbers it needs to animate the arm.
#pragma once

struct WeaponConfig {                // every number you may want to tune
    int   magazineSize = 12;
    int   startReserve = 36;         // 12 + 36 = 48 bullets to start with
    int   maxReserve   = 96;
    float fireInterval = 0.35f;      // seconds between shots (semi-automatic: one shot per click)
    float reloadTime   = 1.6f;       // seconds
    float damage       = 34.0f;      // body shot; a zombie has 100 health, so 3 body shots
    float headshotMultiplier = 2.0f; // head shot = 68 damage
    float range        = 70.0f;      // metres
    float aimInTime    = 0.18f;      // seconds to raise the pistol into the aiming pose
};

// What the player's body needs to animate the pistol.
struct WeaponPose {
    float raise  = 0.0f;             // 0 carried low .. 1 arms up, pistol pointing where the camera looks
    float recoil = 0.0f;             // 1 right after a shot, fades to 0
    float reload = -1.0f;            // -1 not reloading, else 0..1 progress
    float flash  = 0.0f;             // muzzle flash strength (0 = none)
};

class Weapon {
public:
    enum class Fire { Fired, MagazineEmpty, OutOfAmmo, Reloading, Cooldown };

    WeaponConfig cfg;

    Weapon() { reset(); }
    void reset();                              // full starting ammunition, everything idle

    void update(float dt, bool aimHeld);       // timers, aim blend, finishing a reload

    Fire tryFire();                            // consumes ONE bullet if firing is allowed right now
    bool startReload();                        // false if it is not possible (full magazine / no reserve / already reloading)
    void cancelReload() { m_reloading = false; m_reloadT = 0.0f; }
    void addReserve(int rounds);

    int  magazine() const { return m_magazine; }
    int  reserve() const { return m_reserve; }
    bool reloading() const { return m_reloading; }
    float reloadProgress() const { return m_reloading ? m_reloadT / cfg.reloadTime : 0.0f; }
    float aimBlend() const;                    // 0..1, smooth: used for the camera zoom
    float raise() const;                       // 0..1: aiming OR just fired
    float recoil() const { return m_recoil; }
    float muzzleFlash() const { return m_flash > 0.0f ? 1.0f : 0.0f; }
    float timeSinceShot() const { return m_sinceShot; }
    WeaponPose pose() const;

private:
    int m_magazine = 0, m_reserve = 0;
    bool m_reloading = false;
    float m_reloadT = 0.0f, m_cooldown = 0.0f, m_recoil = 0.0f, m_flash = 0.0f, m_sinceShot = 100.0f, m_aim = 0.0f;
};
