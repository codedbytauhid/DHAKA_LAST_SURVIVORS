// src/ui/Font.h
// A built-in 5x7 pixel font (A-Z, 0-9 and common punctuation; lower case is shown as upper case).
// Used by the HUD text (through a texture atlas) and by the signs painted onto the level.
#pragma once
#include "render/Texture.h"

namespace Font {
    const int kAtlasCols = 8;      // the atlas is 8 x 8 cells of 8 x 8 pixels
    const int kCell = 8;

    // 7 rows (top to bottom), 5 bits each (bit 4 = leftmost pixel). Returns blank rows for a space
    // and nullptr for characters we do not have.
    const unsigned char* glyphRows(char c);

    // Cell number of a character inside the atlas, or -1 for a space / unknown character.
    int glyphIndex(char c);

    Texture createAtlas();         // RGBA, white glyphs on transparent, nearest filtering
}
