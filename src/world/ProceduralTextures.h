// src/world/ProceduralTextures.h
// Textures that are drawn by code (no image files needed yet).
// All are 128x128 and tile seamlessly unless noted.
#pragma once
#include <string>
#include <glm/glm.hpp>
#include "render/Texture.h"

namespace ProcTex {
    Texture plaster();       // painted wall: stains, grime near the floor, small chips
    Texture floorTile();     // corridor tiles with grout
    Texture asphalt();       // road
    Texture paving();        // pavers for walkways and pavements
    Texture metal();         // painted metal with rust patches
    Texture foliage();       // leaves
    Texture door();          // panelled wooden door (use with the mesh's own UVs)
    Texture windowGlass();   // RGBA: frame + see-through panes (alpha blended)
    Texture windowDark();    // opaque frame + dark panes (fake windows on far buildings)
    Texture facade();        // apartment/office wall with windows, ledge and an AC unit
    Texture notice();        // paper with lines of "text"
    Texture hazard();        // red/white warning stripes
    Texture gateBars();      // RGBA: iron bars, transparent gaps (cut-out)
    Texture flag();          // Bangladesh flag (160x96, not tiled)

    // A sign made from the built-in 5x7 pixel font (see ui/Font.h: A-Z, 0-9 and common punctuation).
    Texture sign(const std::string& text, glm::vec3 textColor, glm::vec3 backColor);
    float   signAspect(const std::string& text);   // width / height of that texture
}
