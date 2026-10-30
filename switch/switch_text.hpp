#pragma once
// Switch: menu / HUD text in DejaVu Sans (romfs:/fonts, SDL2_ttf) instead of the
// game's 8-px bitmap fonts, which blur together on the 1280x720 handheld screen.
// A file named `classic_font` in sdmc:/switch/ra2engine/ keeps the original fonts.
#include <SDL.h>

#include <string>

// True when the TTF fonts loaded and the classic-font switch is off.
bool sw_text_enabled();
// Width in pixels of `s` at `px` (0 when the fonts are unavailable).
int sw_text_width(const std::string& s, int px, bool bold = false);
// The size at most `px` (and at least `min_px`) at which `s` fits `max_w` pixels.
int sw_text_fit(const std::string& s, int px, int max_w, int min_px = 10, bool bold = false);
// Draws `s` at (x, y) (its top-left), `px` tall, in `col`, with a 1-px black drop
// shadow when `shadow`. Rendered strings are cached as textures.
void sw_text_draw(SDL_Renderer* ren, const std::string& s, int x, int y, int px, SDL_Color col, bool shadow = true,
                  bool bold = false);
// Draws `s` fitted into `box` (shrunk from `px` when too wide), vertically centred;
// align 0 = left, 1 = centre, 2 = right.
void sw_text_box(SDL_Renderer* ren, const std::string& s, const SDL_Rect& box, int align, int px, SDL_Color col,
                 bool shadow = true, bool bold = false);
// Line height in pixels at `px`.
int sw_text_line_h(int px, bool bold = false);
