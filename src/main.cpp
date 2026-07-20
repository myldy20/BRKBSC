// SPDX-License-Identifier: GPL-3.0-or-later
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "audio_runtime.hpp"
#include "core.hpp"
#include "ui.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;
float clamp(float value, float minimum, float maximum) noexcept { return std::max(minimum, std::min(maximum, value)); }

void copy_role(const brkbsc::AccompanimentPlan& source, brkbsc::AccompanimentPlan& target, brkbsc::Role role) {
    switch (role) {
    case brkbsc::Role::Harmony: target.chord_midi = source.chord_midi; break;
    case brkbsc::Role::Bass: target.bass_midi = source.bass_midi; break;
    case brkbsc::Role::Rhythm:
        target.kick = source.kick;
        target.snare = source.snare;
        target.hat = source.hat;
        break;
    case brkbsc::Role::Answer: target.counter_midi = source.counter_midi; break;
    }
}
}

int main(int, char**) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("BRKBSC 0.2 alpha", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kWindowWidth, kWindowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer = window != nullptr ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : nullptr;
    if (window == nullptr || renderer == nullptr) {
        std::fprintf(stderr, "SDL window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, kWindowWidth, kWindowHeight);

    brkbsc::runtime::SharedState shared{};
    brkbsc::runtime::AudioVoice voice(shared);
    SDL_AudioSpec capture_wanted{};
    capture_wanted.freq = brkbsc::kSampleRate;
    capture_wanted.format = AUDIO_F32SYS;
    capture_wanted.channels = 1;
    capture_wanted.samples = 512;
    capture_wanted.callback = brkbsc::runtime::capture_callback;
    capture_wanted.userdata = &shared;

    SDL_AudioSpec playback_wanted{};
    playback_wanted.freq = brkbsc::kSampleRate;
    playback_wanted.format = AUDIO_F32SYS;
    playback_wanted.channels = 2;
    playback_wanted.samples = 512;
    playback_wanted.callback = brkbsc::runtime::playback_callback;
    playback_wanted.userdata = &voice;

    SDL_AudioSpec capture_obtained{};
    SDL_AudioSpec playback_obtained{};
    const SDL_AudioDeviceID capture_device = SDL_OpenAudioDevice(nullptr, SDL_TRUE, &capture_wanted, &capture_obtained, 0);
    const SDL_AudioDeviceID playback_device = SDL_OpenAudioDevice(nullptr, SDL_FALSE, &playback_wanted, &playback_obtained, 0);
    const bool microphone_available = capture_device != 0;
    if (!microphone_available) std::fprintf(stderr, "Microphone unavailable: %s\n", SDL_GetError());
    if (playback_device == 0) {
        std::fprintf(stderr, "Playback unavailable: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (microphone_available) SDL_PauseAudioDevice(capture_device, 0);
    SDL_PauseAudioDevice(playback_device, 0);

    SDL_GameController* controller = nullptr;
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        if (SDL_IsGameController(index)) {
            controller = SDL_GameControllerOpen(index);
            if (controller != nullptr) break;
        }
    }

    brkbsc::PitchDetector detector;
    brkbsc::PhraseRecorder recorder;
    brkbsc::Composer composer;
    std::vector<std::unique_ptr<brkbsc::AccompanimentPlan>> plan_storage;
    plan_storage.reserve(128U);
    std::array<brkbsc::AccompanimentPlan, 4> proposals{};
    std::array<std::uint32_t, 4> proposal_seeds{11U, 23U, 37U, 53U};
    std::array<bool, 4> pinned{};
    std::array<bool, 4> muted{};
    std::vector<brkbsc::NoteEvent> phrase;
    brkbsc::Intent intent{};
    int selected_proposal = 0;
    int selected_role = 0;
    int bpm = 90;
    bool have_phrase = false;
    bool output_enabled = false;

    auto update_mute_mask = [&]() {
        std::uint32_t mask = 0U;
        for (int role = 0; role < 4; ++role) {
            if (muted[static_cast<std::size_t>(role)]) mask |= 1U << static_cast<unsigned>(role);
        }
        shared.role_mute_mask.store(mask, std::memory_order_release);
    };
    auto install_plan = [&](const brkbsc::AccompanimentPlan& plan) {
        plan_storage.push_back(std::make_unique<brkbsc::AccompanimentPlan>(plan));
        shared.plan.store(plan_storage.back().get(), std::memory_order_release);
        shared.reset_counter.fetch_add(1U, std::memory_order_release);
    };
    auto derive_intent = [&]() {
        intent.tension = clamp(0.12F + intent.contrary * 0.76F, 0.0F, 1.0F);
        intent.density = clamp(0.18F + intent.drive * 0.74F, 0.0F, 1.0F);
        intent.evolution = clamp(0.16F + intent.contrary * intent.drive * 0.82F, 0.0F, 1.0F);
    };
    auto generate_proposal = [&](int index, bool preserve_pins, const brkbsc::AccompanimentPlan* source) {
        derive_intent();
        const auto kind = static_cast<brkbsc::ProposalKind>(index);
        auto next = composer.generate(phrase, bpm, ++proposal_seeds[static_cast<std::size_t>(index)], kind, intent);
        if (preserve_pins && source != nullptr) {
            for (int role = 0; role < 4; ++role) {
                if (pinned[static_cast<std::size_t>(role)]) copy_role(*source, next, static_cast<brkbsc::Role>(role));
            }
        }
        proposals[static_cast<std::size_t>(index)] = next;
    };
    auto select_proposal = [&](int index, bool regenerate) {
        if (!have_phrase) return;
        index = (index % 4 + 4) % 4;
        const brkbsc::AccompanimentPlan* source = &proposals[static_cast<std::size_t>(selected_proposal)];
        if (regenerate || index != selected_proposal) generate_proposal(index, true, source);
        selected_proposal = index;
        install_plan(proposals[static_cast<std::size_t>(selected_proposal)]);
        output_enabled = true;
        shared.output_enabled.store(true, std::memory_order_release);
    };

    std::array<float, 2048> analysis_block{};
    brkbsc::AnalysisFrame latest{};
    bool running = true;
    const Uint64 performance_frequency = SDL_GetPerformanceFrequency();
    const Uint64 started = SDL_GetPerformanceCounter();
    auto now_seconds = [&]() {
        return static_cast<double>(SDL_GetPerformanceCounter() - started) / static_cast<double>(performance_frequency);
    };

    auto start_capture = [&]() {
        recorder.start(now_seconds(), bpm);
        output_enabled = false;
        shared.output_enabled.store(false, std::memory_order_release);
        shared.plan.store(nullptr, std::memory_order_release);
        shared.reset_counter.fetch_add(1U, std::memory_order_release);
        pinned.fill(false);
        muted.fill(false);
        update_mute_mask();
    };
    auto stop_capture = [&]() {
        recorder.stop(now_seconds());
        phrase = recorder.notes();
        have_phrase = !phrase.empty();
        if (!have_phrase) return;
        for (int index = 0; index < 4; ++index) generate_proposal(index, false, nullptr);
        selected_proposal = 0;
        install_plan(proposals[0]);
        output_enabled = true;
        shared.output_enabled.store(true, std::memory_order_release);
    };
    auto toggle_primary = [&]() {
        if (shared.mode.load() != 0) {
            output_enabled = !output_enabled;
            shared.output_enabled.store(output_enabled, std::memory_order_release);
            return;
        }
        if (!recorder.recording()) start_capture(); else stop_capture();
    };
    auto change_mode = [&]() {
        const int next = shared.mode.load() == 0 ? 1 : 0;
        shared.mode.store(next);
        shared.reset_counter.fetch_add(1U, std::memory_order_release);
        if (recorder.recording()) recorder.stop(now_seconds());
        if (next == 1) {
            output_enabled = true;
            shared.output_enabled.store(true, std::memory_order_release);
        } else {
            output_enabled = have_phrase;
            shared.output_enabled.store(output_enabled, std::memory_order_release);
            if (have_phrase) install_plan(proposals[static_cast<std::size_t>(selected_proposal)]);
        }
    };
    auto toggle_freeze_or_pin = [&]() {
        if (shared.mode.load() == 0) {
            pinned[static_cast<std::size_t>(selected_role)] = !pinned[static_cast<std::size_t>(selected_role)];
        } else {
            const bool frozen = !shared.frozen.load();
            if (frozen) shared.frozen_pitch_hz.store(std::max(55.0F, latest.pitch_hz));
            shared.frozen.store(frozen);
        }
    };
    auto secondary_action = [&]() {
        if (shared.mode.load() == 0) select_proposal(selected_proposal + 1, true);
        else shared.reset_counter.fetch_add(1U, std::memory_order_release);
    };
    auto toggle_selected_mute = [&]() {
        if (shared.mode.load() != 0) return;
        muted[static_cast<std::size_t>(selected_role)] = !muted[static_cast<std::size_t>(selected_role)];
        update_mute_mask();
    };
    auto move_intent = [&](float horizontal, float vertical) {
        if (shared.mode.load() == 0) {
            intent.contrary = clamp(intent.contrary + horizontal, 0.0F, 1.0F);
            intent.drive = clamp(intent.drive + vertical, 0.0F, 1.0F);
            if (have_phrase) select_proposal(selected_proposal, true);
        } else {
            if (horizontal != 0.0F) shared.agency.store(clamp(shared.agency.load() + horizontal, 0.0F, 1.0F));
            if (vertical != 0.0F) bpm = std::max(40, std::min(180, bpm + (vertical > 0.0F ? 2 : -2)));
        }
    };

    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) running = false;
            else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                const SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE) running = false;
                else if (key == SDLK_SPACE) toggle_primary();
                else if (key == SDLK_TAB || key == SDLK_x) change_mode();
                else if (key == SDLK_BACKSPACE || key == SDLK_b) secondary_action();
                else if (key == SDLK_f || key == SDLK_y || key == SDLK_p) toggle_freeze_or_pin();
                else if (key == SDLK_m) toggle_selected_mute();
                else if (key == SDLK_r && shared.mode.load() == 0) select_proposal(selected_proposal, true);
                else if (key >= SDLK_1 && key <= SDLK_4 && shared.mode.load() == 0) select_proposal(static_cast<int>(key - SDLK_1), true);
                else if (key == SDLK_q && shared.mode.load() == 0) selected_role = (selected_role + 3) % 4;
                else if (key == SDLK_e && shared.mode.load() == 0) selected_role = (selected_role + 1) % 4;
                else if (key == SDLK_RETURN) {
                    output_enabled = !output_enabled;
                    shared.output_enabled.store(output_enabled, std::memory_order_release);
                } else if (key == SDLK_LEFTBRACKET) bpm = std::max(40, bpm - 2);
                else if (key == SDLK_RIGHTBRACKET) bpm = std::min(180, bpm + 2);
                else if (key == SDLK_LEFT) move_intent(-0.08F, 0.0F);
                else if (key == SDLK_RIGHT) move_intent(0.08F, 0.0F);
                else if (key == SDLK_UP) move_intent(0.0F, 0.08F);
                else if (key == SDLK_DOWN) move_intent(0.0F, -0.08F);
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A: toggle_primary(); break;
                case SDL_CONTROLLER_BUTTON_B: secondary_action(); break;
                case SDL_CONTROLLER_BUTTON_X: change_mode(); break;
                case SDL_CONTROLLER_BUTTON_Y: toggle_freeze_or_pin(); break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: if (shared.mode.load() == 0) selected_role = (selected_role + 3) % 4; break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: if (shared.mode.load() == 0) selected_role = (selected_role + 1) % 4; break;
                case SDL_CONTROLLER_BUTTON_BACK: toggle_selected_mute(); break;
                case SDL_CONTROLLER_BUTTON_START:
                    output_enabled = !output_enabled;
                    shared.output_enabled.store(output_enabled, std::memory_order_release);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: move_intent(-0.08F, 0.0F); break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: move_intent(0.08F, 0.0F); break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP: move_intent(0.0F, 0.08F); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: move_intent(0.0F, -0.08F); break;
                default: break;
                }
            }
        }

        if (shared.analysis_input.available() >= analysis_block.size()) {
            shared.analysis_input.pop(analysis_block.data(), analysis_block.size());
            latest = detector.analyse(analysis_block.data(), analysis_block.size(), brkbsc::kSampleRate);
            shared.rms.store(latest.rms, std::memory_order_relaxed);
            shared.pitch_hz.store(latest.pitch_hz, std::memory_order_relaxed);
            shared.pitch_confidence.store(latest.pitch_confidence, std::memory_order_relaxed);
            if (latest.onset) shared.onset_counter.fetch_add(1U, std::memory_order_release);
            recorder.process(latest, now_seconds());
        }

        const int mode = shared.mode.load();
        const float agency = shared.agency.load();
        SDL_SetRenderDrawColor(renderer, 13, 15, 18, 255);
        SDL_RenderClear(renderer);
        SDL_Rect top{0, 0, kWindowWidth, 92};
        SDL_SetRenderDrawColor(renderer, 27, 30, 35, 255);
        SDL_RenderFillRect(renderer, &top);
        brkbsc::ui::draw_text(renderer, 38, 26, "BRKBSC", SDL_Color{236, 229, 200, 255}, 6);
        brkbsc::ui::draw_text(renderer, 735, 34, mode == 0 ? "COMPOSE" : "LIVE", SDL_Color{185, 200, 194, 255}, 4);
        brkbsc::ui::draw_text(renderer, 48, 126, microphone_available ? "MIC ONLINE" : "MIC OFFLINE", SDL_Color{150, 164, 159, 255}, 3);
        brkbsc::ui::draw_meter(renderer, 48, 168, 480, 28, latest.rms * 8.0F);
        brkbsc::ui::draw_text(renderer, 560, 166, brkbsc::ui::note_name(latest.pitch_hz), SDL_Color{236, 229, 200, 255}, 4);

        if (mode == 0) {
            if (recorder.recording()) {
                brkbsc::ui::draw_text(renderer, 48, 245, "LISTENING IN SILENCE", SDL_Color{220, 194, 156, 255}, 5);
                brkbsc::ui::draw_text(renderer, 48, 310, "A STOP AND COMPOSE", SDL_Color{135, 145, 151, 255}, 3);
            } else if (!have_phrase) {
                brkbsc::ui::draw_text(renderer, 48, 245, "HUM A MUSICAL IDEA", SDL_Color{220, 194, 156, 255}, 5);
                brkbsc::ui::draw_text(renderer, 48, 310, "A RECORD", SDL_Color{135, 145, 151, 255}, 3);
            } else {
                const auto& plan = proposals[static_cast<std::size_t>(selected_proposal)];
                brkbsc::ui::draw_text(renderer, 48, 235, brkbsc::proposal_name(plan.kind), SDL_Color{220, 194, 156, 255}, 5);
                const std::string key = std::string("KEY ") + brkbsc::pitch_class_name(plan.tonal.root_pitch_class) + (plan.tonal.minor ? " MINOR" : " MAJOR");
                brkbsc::ui::draw_text(renderer, 48, 292, key, SDL_Color{185, 200, 194, 255}, 3);

                brkbsc::ui::draw_text(renderer, 48, 350, "ROLES", SDL_Color{185, 200, 194, 255}, 3);
                for (int role = 0; role < 4; ++role) {
                    std::string row = role == selected_role ? "> " : "  ";
                    row += brkbsc::role_name(static_cast<brkbsc::Role>(role));
                    if (pinned[static_cast<std::size_t>(role)]) row += " PIN";
                    if (muted[static_cast<std::size_t>(role)]) row += " MUTE";
                    brkbsc::ui::draw_text(renderer, 48, 392 + role * 42, row,
                        role == selected_role ? SDL_Color{236, 229, 200, 255} : SDL_Color{135, 145, 151, 255}, 3);
                }

                SDL_Rect compass{590, 270, 350, 260};
                SDL_SetRenderDrawColor(renderer, 72, 77, 84, 255);
                SDL_RenderDrawRect(renderer, &compass);
                SDL_RenderDrawLine(renderer, compass.x + compass.w / 2, compass.y, compass.x + compass.w / 2, compass.y + compass.h);
                SDL_RenderDrawLine(renderer, compass.x, compass.y + compass.h / 2, compass.x + compass.w, compass.y + compass.h / 2);
                const int marker_x = compass.x + static_cast<int>(intent.contrary * static_cast<float>(compass.w));
                const int marker_y = compass.y + compass.h - static_cast<int>(intent.drive * static_cast<float>(compass.h));
                SDL_Rect marker{marker_x - 7, marker_y - 7, 14, 14};
                SDL_SetRenderDrawColor(renderer, 223, 213, 170, 255);
                SDL_RenderFillRect(renderer, &marker);
                brkbsc::ui::draw_text(renderer, 590, 235, "STILL / DRIVING", SDL_Color{150, 164, 159, 255}, 2);
                brkbsc::ui::draw_text(renderer, 590, 545, "FAITHFUL", SDL_Color{120, 132, 138, 255}, 2);
                brkbsc::ui::draw_text(renderer, 825, 545, "CONTRARY", SDL_Color{120, 132, 138, 255}, 2);
            }
            brkbsc::ui::draw_text(renderer, 48, 650, "A CAPTURE  B PROPOSAL  Q/E ROLE  P PIN  M MUTE  R REGEN", SDL_Color{100, 111, 117, 255}, 2);
        } else {
            brkbsc::ui::draw_text(renderer, 48, 270, shared.frozen.load() ? "GHOST FROZEN" : "GHOST LISTENING", SDL_Color{220, 194, 156, 255}, 5);
            brkbsc::ui::draw_text(renderer, 48, 340, "Y FREEZE TONE", SDL_Color{135, 145, 151, 255}, 3);
            brkbsc::ui::draw_text(renderer, 48, 385, "B ERASE MEMORY", SDL_Color{135, 145, 151, 255}, 3);
            brkbsc::ui::draw_text(renderer, 48, 470, "AGENCY", SDL_Color{185, 200, 194, 255}, 4);
            brkbsc::ui::draw_meter(renderer, 260, 470, 430, 34, agency);
            brkbsc::ui::draw_text(renderer, 48, 650, "X MODE  A GHOST  Y FREEZE  B ERASE  DPAD TEMPO / AGENCY", SDL_Color{100, 111, 117, 255}, 2);
        }
        brkbsc::ui::draw_text(renderer, 850, 650, std::to_string(bpm) + " BPM", SDL_Color{150, 164, 159, 255}, 2);
        SDL_RenderPresent(renderer);
    }

    SDL_PauseAudioDevice(playback_device, 1);
    SDL_CloseAudioDevice(playback_device);
    if (microphone_available) {
        SDL_PauseAudioDevice(capture_device, 1);
        SDL_CloseAudioDevice(capture_device);
    }
    if (controller != nullptr) SDL_GameControllerClose(controller);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
