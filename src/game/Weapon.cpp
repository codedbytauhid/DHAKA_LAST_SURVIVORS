// src/game/Weapon.cpp
#include "game/Weapon.h"
#include <algorithm>

static float smooth(float t) { t = std::max(0.0f, std::min(1.0f, t)); return t * t * (3.0f - 2.0f * t); }

void Weapon::reset()
{
    m_magazine = cfg.magazineSize;
    m_reserve = cfg.startReserve;
    m_reloading = false;
    m_reloadT = m_cooldown = m_recoil = m_flash = m_aim = 0.0f;
    m_sinceShot = 100.0f;
}

void Weapon::update(float dt, bool aimHeld)
{
    m_cooldown = std::max(0.0f, m_cooldown - dt);
    m_recoil   = std::max(0.0f, m_recoil - dt * 6.0f);
    m_flash    = std::max(0.0f, m_flash - dt);
    m_sinceShot += dt;

    float step = dt / std::max(0.01f, cfg.aimInTime);
    m_aim = aimHeld ? std::min(1.0f, m_aim + step) : std::max(0.0f, m_aim - step);

    if (m_reloading) {
        m_reloadT += dt;
        if (m_reloadT >= cfg.reloadTime) {                  // reload finished: move bullets from the reserve into the magazine
            int need = cfg.magazineSize - m_magazine;
            int take = std::min(need, m_reserve);
            m_magazine += take;
            m_reserve -= take;
            m_reloading = false;
            m_reloadT = 0.0f;
        }
    }
}

Weapon::Fire Weapon::tryFire()
{
    if (m_reloading) return Fire::Reloading;
    if (m_cooldown > 0.0f) return Fire::Cooldown;
    if (m_magazine <= 0) return (m_reserve > 0) ? Fire::MagazineEmpty : Fire::OutOfAmmo;
    --m_magazine;
    m_cooldown = cfg.fireInterval;
    m_recoil = 1.0f;
    m_flash = 0.06f;
    m_sinceShot = 0.0f;
    return Fire::Fired;
}

bool Weapon::startReload()
{
    if (m_reloading || m_magazine >= cfg.magazineSize || m_reserve <= 0) return false;
    m_reloading = true;
    m_reloadT = 0.0f;
    return true;
}

void Weapon::addReserve(int rounds)
{
    m_reserve = std::min(cfg.maxReserve, m_reserve + std::max(0, rounds));
}

float Weapon::aimBlend() const { return smooth(m_aim); }

float Weapon::raise() const
{
    // the pistol stays up for half a second after a shot, then lowers again
    float afterShot = smooth(1.0f - (m_sinceShot - 0.5f) / 0.4f);
    return std::max(aimBlend(), afterShot);
}

WeaponPose Weapon::pose() const
{
    WeaponPose p;
    p.raise = raise();
    p.recoil = m_recoil;
    p.reload = m_reloading ? reloadProgress() : -1.0f;
    p.flash = muzzleFlash();
    return p;
}
