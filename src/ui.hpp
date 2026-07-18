// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <SDL.h>
#include <string>

namespace brkbsc::ui {
void draw_text(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color colour, int scale);
void draw_meter(SDL_Renderer* renderer, int x, int y, int width, int height, float value);
std::string note_name(float pitch_hz);
} // namespace brkbsc::ui
