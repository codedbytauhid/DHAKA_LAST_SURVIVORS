// src/world/Level.h
// The whole university environment: classroom, corridor, staircase, courtyard, gate, road.
// A Level owns its textures, one cube Mesh, a list of "props" (boxes) and a list of point lights.
// Everything is built from the ONE cube mesh: each prop = position + size + rotation + material.
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

#include "render/Mesh.h"
#include "render/Shader.h"
#include "render/Texture.h"
#include "world/Collision.h"

struct Prop {
    glm::vec3 pos{0.0f};            // centre
    glm::vec3 size{1.0f};           // width, height, depth
    glm::vec3 rot{0.0f};            // degrees: x, y, z (applied as Ry * Rx * Rz)
    const Texture* tex = nullptr;
    glm::vec2 uv{1.0f};             // texture repeats
    glm::vec3 tint{1.0f};
    float shininess = 16.0f;
    float specular = 0.05f;
    float sunScale = 1.0f;          // how much sun this object receives (indoor = little)
    float ambientScale = 1.0f;      // how much ambient light (corridor = darker)
    float emissive = 0.0f;          // self-lit amount
    float alpha = 1.0f;
    int uvMode = 0;                 // 0 world-space UVs, 1 mesh UVs
    int mode = 0;                   // 0 opaque, 1 cut-out, 2 alpha-blended
    int light = -1;                 // index of the point light this prop's glow follows (flicker)
    bool solid = true;              // true = the player collides with it (and the camera cannot see through it)
    int tag = 0;                    // mission props: 1 card, 2 gate-left, 3 gate-right, 4 panel-red, 5 panel-green, 6 checkpoint, 7 "closed" sign, 8..11 ammo crates
    bool visible = true;            // hidden props are neither drawn nor (for tagged ones) solid
    glm::vec3 offset{0.0f};         // extra movement applied when drawing (sliding gate, bobbing card)
};

struct PointLight {
    glm::vec3 pos{0.0f};
    glm::vec3 color{1.0f};
    float radius = 8.0f;
    int flicker = 0;                // 0 steady, 1 gentle, 2 broken (random blackouts), 3 pulsing emergency
    float phase = 0.0f;
    float intensity = 1.0f;         // updated every frame
    bool enabled = true;            // switched off lights are ignored
};

class Level {
public:
    Level();                        // needs a current OpenGL context (creates textures + mesh)

    // Drawing is split in two so the player can be drawn between the solid world and the glass:
    //   drawOpaque(): updates flicker, sends the 8 nearest lights, draws opaque + cut-out props
    //   drawGlass():  draws windows (alpha blended, sorted far -> near)
    // The caller must already have called shader.use() and set camera/sun/fog uniforms.
    void drawOpaque(const Shader& shader, const glm::vec3& cameraPos, float time);
    void drawGlass(const Shader& shader, const glm::vec3& cameraPos);

    // Collision boxes made from every solid prop (built once in the constructor).
    const CollisionWorld& collision() const { return m_collision; }

    // How much sun/ambient light a character standing at `p` should receive (indoors = less).
    void lightingAt(const glm::vec3& p, float& sunScale, float& ambientScale) const;

    // ---- mission props (all cheap: nothing happens if the value did not change)
    void setCardVisible(bool visible);        // the access card, its beacon and its light
    void setPanelUnlocked(bool unlocked);     // booth terminal: red light -> green light
    void setGateProgress(float t);            // 0 closed .. 1 fully open; the gate stops blocking once it is mostly open
    void setCheckpointActive(bool active);    // the glowing safe checkpoint on the road
    void setAmmoVisible(int index, bool visible);   // ammunition crate 0..3 (hidden once collected)
    void resetMission();

    // Read-only access to the point lights (the audio system listens to the broken ones blinking).
    const std::vector<PointLight>& lights() const { return m_lights; }
    // A short orange glow where the pistol fires. strength 0 = off. (It is one extra point light, so it lights walls and zombies.)
    void setMuzzleLight(const glm::vec3& position, float strength);                      // card back on the table, gate closed, panel red, checkpoint hidden

    size_t propCount() const { return m_props.size(); }
    size_t lightCount() const { return m_lights.size(); }

private:
    void drawProp(const Shader& shader, const Prop& p) const;

    Mesh m_cube;
    std::unordered_map<std::string, Texture> m_tex;
    std::vector<Prop> m_props;
    std::vector<PointLight> m_lights;
    CollisionWorld m_collision;
    std::vector<int> m_tagProps[16];          // prop indices for every mission tag
    std::vector<int> m_gateCollision;        // collision box ids of the two gate leaves
    int m_cardLight = -1, m_checkpointLight = -1, m_flashLight = -1;
    float m_gateProgress = -1.0f;
    void setTagVisible(int tag, bool visible);
    mutable const Texture* m_boundTex = nullptr;
};
