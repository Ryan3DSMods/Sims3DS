/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — built-in anti-aliased bitmap fonts (FontData.h, generated from Inter, OFL 1.1).
// Text is Latin-1 / Windows-1252 bytes, as in the game's STR# tables.
#pragma once
#include <string>
#include <vector>
#include "app/Canvas.h"

namespace simapp {

enum class FontId { Small, Bold, Title };

int TextWidth(FontId f, const std::string& s);
int LineHeight(FontId f);
/// Draws text with its top at y. Returns the x after the last glyph.
int DrawText(Canvas& c, FontId f, int x, int y, const std::string& s, uint32_t color);
/// Draws text centred in the rect (vertically too).
void DrawTextCentered(Canvas& c, FontId f, const Rect& r, const std::string& s, uint32_t color);
/// Draws text with a 1px dark outline/shadow (for text over the lot view).
int DrawTextShadow(Canvas& c, FontId f, int x, int y, const std::string& s, uint32_t color, uint32_t shadow);
/// Shortens s with "..." so it fits in maxWidth pixels.
std::string Ellipsize(FontId f, const std::string& s, int maxWidth);
/// Word-wraps text (also on \n) into lines no wider than maxWidth.
std::vector<std::string> WrapText(FontId f, const std::string& s, int maxWidth);
/// Converts a UTF-8 string (if it is one) to Latin-1; plain bytes otherwise.
std::string ToLatin1(const std::string& s);

}
