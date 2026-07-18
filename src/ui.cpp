// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui.hpp"
#include "core.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>

namespace brkbsc::ui {
namespace {
std::array<std::uint8_t, 7> glyph(char c) {
    using G = std::array<std::uint8_t, 7>;
    switch (c) {
    case 'A': return G{14,17,17,31,17,17,17}; case 'B': return G{30,17,17,30,17,17,30};
    case 'C': return G{14,17,16,16,16,17,14}; case 'D': return G{30,17,17,17,17,17,30};
    case 'E': return G{31,16,16,30,16,16,31}; case 'F': return G{31,16,16,30,16,16,16};
    case 'G': return G{14,17,16,23,17,17,14}; case 'H': return G{17,17,17,31,17,17,17};
    case 'I': return G{31,4,4,4,4,4,31}; case 'J': return G{7,2,2,2,18,18,12};
    case 'K': return G{17,18,20,24,20,18,17}; case 'L': return G{16,16,16,16,16,16,31};
    case 'M': return G{17,27,21,21,17,17,17}; case 'N': return G{17,25,21,19,17,17,17};
    case 'O': return G{14,17,17,17,17,17,14}; case 'P': return G{30,17,17,30,16,16,16};
    case 'Q': return G{14,17,17,17,21,18,13}; case 'R': return G{30,17,17,30,20,18,17};
    case 'S': return G{15,16,16,14,1,1,30}; case 'T': return G{31,4,4,4,4,4,4};
    case 'U': return G{17,17,17,17,17,17,14}; case 'V': return G{17,17,17,17,17,10,4};
    case 'W': return G{17,17,17,21,21,21,10}; case 'X': return G{17,17,10,4,10,17,17};
    case 'Y': return G{17,17,10,4,4,4,4}; case 'Z': return G{31,1,2,4,8,16,31};
    case '0': return G{14,17,19,21,25,17,14}; case '1': return G{4,12,4,4,4,4,14};
    case '2': return G{14,17,1,2,4,8,31}; case '3': return G{30,1,1,14,1,1,30};
    case '4': return G{2,6,10,18,31,2,2}; case '5': return G{31,16,16,30,1,1,30};
    case '6': return G{14,16,16,30,17,17,14}; case '7': return G{31,1,2,4,8,8,8};
    case '8': return G{14,17,17,14,17,17,14}; case '9': return G{14,17,17,15,1,1,14};
    case '#': return G{10,31,10,10,31,10,0}; case '-': return G{0,0,0,31,0,0,0};
    case ':': return G{0,4,4,0,4,4,0}; case '/': return G{1,2,2,4,8,8,16};
    case '.': return G{0,0,0,0,0,4,4}; default: return G{};
    }
}
}

void draw_text(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color colour, int scale) {
    SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, colour.a);
    int cursor = x;
    for (char character : text) {
        if (character == ' ') {
            cursor += 6 * scale;
            continue;
        }
        const auto rows = glyph(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[static_cast<std::size_t>(row)] & static_cast<std::uint8_t>(1U << (4 - column))) != 0U) {
                    SDL_Rect pixel{cursor + column * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
        cursor += 6 * scale;
    }
}

void draw_meter(SDL_Renderer* renderer, int x, int y, int width, int height, float value) {
    SDL_Rect outline{x, y, width, height};
    SDL_SetRenderDrawColor(renderer, 72, 77, 84, 255);
    SDL_RenderDrawRect(renderer, &outline);
    SDL_Rect fill{x + 2, y + 2, static_cast<int>((width - 4) * std::max(0.0F, std::min(1.0F, value))), height - 4};
    SDL_SetRenderDrawColor(renderer, 223, 213, 170, 255);
    SDL_RenderFillRect(renderer, &fill);
}

std::string note_name(float pitch_hz) {
    const int midi = hz_to_midi(pitch_hz);
    if (midi < 0) return "--";
    return std::string(pitch_class_name(midi % 12)) + std::to_string(midi / 12 - 1);
}

} // namespace brkbsc::ui
