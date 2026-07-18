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
}

int main(int, char**) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("BRKBSC alpha", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kWindowWidth, kWindowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer = window != nullptr
        ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
        : nullptr;
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
    std::vector<std::unique_ptr<brkbsc::AccompanimentPlan>> plans;
    auto install_plan = [&](brkbsc::AccompanimentPlan plan) {
        plans.push_back(std::make_unique<brkbsc::AccompanimentPlan>(std::move(plan)));
        shared.plan.store(plans.back().get(), std::memory_order_release);
    };

    int bpm = 90;
    std::uint32_t seed = 1U;
    install_plan(composer.generate({}, bpm, seed));
    std::array<float, 2048> analysis_block{};
    brkbsc::AnalysisFrame latest{};
    bool running = true;
    bool output_enabled = true;
    const Uint64 performance_frequency = SDL_GetPerformanceFrequency();
    const Uint64 started = SDL_GetPerformanceCounter();
    auto now_seconds = [&]() {
        return static_cast<double>(SDL_GetPerformanceCounter() - started) / static_cast<double>(performance_frequency);
    };

    auto toggle_primary = [&]() {
        if (shared.mode.load() != 0) {
            output_enabled = !output_enabled;
            shared.output_enabled.store(output_enabled);
            return;
        }
        if (!recorder.recording()) {
            recorder.start(now_seconds(), bpm);
        } else {
            recorder.stop(now_seconds());
            install_plan(composer.generate(recorder.notes(), bpm, ++seed));
            output_enabled = true;
            shared.output_enabled.store(true);
        }
    };

    auto change_mode = [&]() {
        shared.mode.store(shared.mode.load() == 0 ? 1 : 0);
        shared.reset_counter.fetch_add(1U);
        if (recorder.recording()) recorder.stop(now_seconds());
    };

    auto secondary_action = [&]() {
        if (shared.mode.load() == 0) install_plan(composer.generate(recorder.notes(), bpm, ++seed));
        else shared.reset_counter.fetch_add(1U);
    };

    auto toggle_freeze = [&]() {
        const bool frozen = !shared.frozen.load();
        if (frozen) shared.frozen_pitch_hz.store(std::max(55.0F, latest.pitch_hz));
        shared.frozen.store(frozen);
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
                else if (key == SDLK_f || key == SDLK_y) toggle_freeze();
                else if (key == SDLK_RETURN) {
                    output_enabled = !output_enabled;
                    shared.output_enabled.store(output_enabled);
                } else if (key == SDLK_UP) bpm = std::min(180, bpm + 2);
                else if (key == SDLK_DOWN) bpm = std::max(40, bpm - 2);
                else if (key == SDLK_LEFT) shared.agency.store(clamp(shared.agency.load() - 0.05F, 0.0F, 1.0F));
                else if (key == SDLK_RIGHT) shared.agency.store(clamp(shared.agency.load() + 0.05F, 0.0F, 1.0F));
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A: toggle_primary(); break;
                case SDL_CONTROLLER_BUTTON_B: secondary_action(); break;
                case SDL_CONTROLLER_BUTTON_X: change_mode(); break;
                case SDL_CONTROLLER_BUTTON_Y: toggle_freeze(); break;
                case SDL_CONTROLLER_BUTTON_START:
                    output_enabled = !output_enabled;
                    shared.output_enabled.store(output_enabled);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP: bpm = std::min(180, bpm + 2); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: bpm = std::max(40, bpm - 2); break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: shared.agency.store(clamp(shared.agency.load() - 0.05F, 0.0F, 1.0F)); break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: shared.agency.store(clamp(shared.agency.load() + 0.05F, 0.0F, 1.0F)); break;
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
        brkbsc::ui::draw_text(renderer, 48, 135, microphone_available ? "MIC ONLINE" : "MIC OFFLINE", SDL_Color{150, 164, 159, 255}, 3);
        brkbsc::ui::draw_meter(renderer, 48, 180, 560, 34, latest.rms * 8.0F);
        brkbsc::ui::draw_text(renderer, 640, 178, brkbsc::ui::note_name(latest.pitch_hz), SDL_Color{236, 229, 200, 255}, 4);

        if (mode == 0) {
            brkbsc::ui::draw_text(renderer, 48, 270, recorder.recording() ? "LISTENING" : "HUM A PHRASE", SDL_Color{220, 194, 156, 255}, 5);
            brkbsc::ui::draw_text(renderer, 48, 340, "A RECORD / STOP", SDL_Color{135, 145, 151, 255}, 3);
            brkbsc::ui::draw_text(renderer, 48, 385, "B NEW ACCOMPANIMENT", SDL_Color{135, 145, 151, 255}, 3);
            const auto* plan = shared.plan.load();
            if (plan != nullptr) {
                const std::string key = std::string("KEY ") + brkbsc::pitch_class_name(plan->tonal.root_pitch_class) + (plan->tonal.minor ? " MINOR" : " MAJOR");
                brkbsc::ui::draw_text(renderer, 48, 470, key, SDL_Color{185, 200, 194, 255}, 4);
            }
        } else {
            brkbsc::ui::draw_text(renderer, 48, 270, shared.frozen.load() ? "GHOST FROZEN" : "GHOST LISTENING", SDL_Color{220, 194, 156, 255}, 5);
            brkbsc::ui::draw_text(renderer, 48, 340, "Y FREEZE TONE", SDL_Color{135, 145, 151, 255}, 3);
            brkbsc::ui::draw_text(renderer, 48, 385, "B ERASE MEMORY", SDL_Color{135, 145, 151, 255}, 3);
            brkbsc::ui::draw_text(renderer, 48, 470, "AGENCY", SDL_Color{185, 200, 194, 255}, 4);
            brkbsc::ui::draw_meter(renderer, 260, 470, 430, 34, agency);
        }
        brkbsc::ui::draw_text(renderer, 48, 650, "X MODE   START SOUND   DPAD TEMPO / AGENCY", SDL_Color{100, 111, 117, 255}, 2);
        brkbsc::ui::draw_text(renderer, 820, 650, std::to_string(bpm) + " BPM", SDL_Color{150, 164, 159, 255}, 2);
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
