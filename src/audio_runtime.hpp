// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <SDL.h>
#include "core.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace brkbsc::runtime {

template <std::size_t Capacity>
class SampleRing {
public:
    void push(const float* source, std::size_t count) noexcept {
        auto write = write_.load(std::memory_order_relaxed);
        auto read = read_.load(std::memory_order_acquire);
        for (std::size_t index = 0; index < count; ++index) {
            const auto next = (write + 1U) % Capacity;
            if (next == read) {
                read = (read + 1U) % Capacity;
                read_.store(read, std::memory_order_release);
            }
            data_[write] = source[index];
            write = next;
        }
        write_.store(write, std::memory_order_release);
    }

    std::size_t pop(float* destination, std::size_t count) noexcept {
        auto read = read_.load(std::memory_order_relaxed);
        const auto write = write_.load(std::memory_order_acquire);
        std::size_t popped = 0U;
        while (popped < count && read != write) {
            destination[popped++] = data_[read];
            read = (read + 1U) % Capacity;
        }
        read_.store(read, std::memory_order_release);
        return popped;
    }

    [[nodiscard]] std::size_t available() const noexcept {
        const auto read = read_.load(std::memory_order_acquire);
        const auto write = write_.load(std::memory_order_acquire);
        return write >= read ? write - read : Capacity - read + write;
    }

private:
    std::array<float, Capacity> data_{};
    std::atomic<std::size_t> read_{0U};
    std::atomic<std::size_t> write_{0U};
};

struct SharedState {
    SampleRing<131072U> analysis_input{};
    SampleRing<131072U> live_input{};
    std::atomic<float> rms{0.0F};
    std::atomic<float> pitch_hz{0.0F};
    std::atomic<float> pitch_confidence{0.0F};
    std::atomic<std::uint32_t> onset_counter{0U};
    std::atomic<const AccompanimentPlan*> plan{nullptr};
    std::atomic<int> mode{0};
    std::atomic<bool> output_enabled{false};
    std::atomic<bool> frozen{false};
    std::atomic<float> frozen_pitch_hz{110.0F};
    std::atomic<float> agency{0.45F};
    std::atomic<std::uint32_t> role_mute_mask{0U};
    std::atomic<std::uint32_t> reset_counter{0U};
};

class AudioVoice {
public:
    explicit AudioVoice(SharedState& state);
    void render(float* output, int frames, int channels) noexcept;
private:
    float advance(float& phase, float frequency) noexcept;
    void trigger_step(const AccompanimentPlan& plan, int step) noexcept;
    void reset_transport() noexcept;
    float render_compose() noexcept;
    float render_live() noexcept;

    SharedState& state_;
    std::vector<float> delay_{};
    std::size_t delay_write_ = 0U;
    std::uint32_t seen_reset_ = 0U;
    std::uint32_t seen_onset_ = 0U;
    std::uint32_t noise_state_ = 0x12345678U;
    bool event_flip_ = false;
    double step_samples_ = 0.0;
    int step_ = 0;
    float bass_frequency_ = 65.4F;
    float counter_frequency_ = 261.6F;
    std::array<float, kChordVoices> chord_frequency_{130.8F, 164.8F, 196.0F, 246.9F};
    float bass_phase_ = 0.0F;
    float counter_phase_ = 0.0F;
    std::array<float, kChordVoices> chord_phase_{};
    float kick_phase_ = 0.0F;
    float bass_env_ = 0.0F;
    float counter_env_ = 0.0F;
    float kick_env_ = 0.0F;
    float snare_env_ = 0.0F;
    float hat_env_ = 0.0F;
    float previous_noise_ = 0.0F;
    float live_pitch_ = 110.0F;
    float live_phase_a_ = 0.0F;
    float live_phase_b_ = 0.0F;
    float live_pulse_phase_ = 0.0F;
    float live_pulse_env_ = 0.0F;
    float drone_env_ = 0.0F;
};

void capture_callback(void* userdata, Uint8* stream, int length);
void playback_callback(void* userdata, Uint8* stream, int length);

} // namespace brkbsc::runtime
