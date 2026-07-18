// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_runtime.hpp"

#include <algorithm>
#include <cmath>

namespace brkbsc::runtime {
namespace {
constexpr float kPi = 3.14159265358979323846F;
float clamp(float value, float minimum, float maximum) noexcept { return std::max(minimum, std::min(maximum, value)); }
float soft_clip(float sample) noexcept { return sample / (1.0F + std::abs(sample)); }
float triangle(float phase) noexcept { const float wrapped = phase - std::floor(phase); return 4.0F * std::abs(wrapped - 0.5F) - 1.0F; }
}

AudioVoice::AudioVoice(SharedState& state)
    : state_(state), delay_(static_cast<std::size_t>(kSampleRate * 4), 0.0F) {}

float AudioVoice::advance(float& phase, float frequency) noexcept {
    phase += frequency / static_cast<float>(kSampleRate);
    phase -= std::floor(phase);
    return phase;
}

void AudioVoice::render(float* output, int frames, int channels) noexcept {
    const bool enabled = state_.output_enabled.load(std::memory_order_relaxed);
    const int mode = state_.mode.load(std::memory_order_relaxed);
    const auto reset = state_.reset_counter.load(std::memory_order_acquire);
    if (reset != seen_reset_) {
        std::fill(delay_.begin(), delay_.end(), 0.0F);
        seen_reset_ = reset;
    }
    for (int frame = 0; frame < frames; ++frame) {
        float sample = enabled ? (mode == 0 ? render_compose() : render_live()) : 0.0F;
        sample = soft_clip(sample * 0.82F);
        for (int channel = 0; channel < channels; ++channel) output[frame * channels + channel] = sample;
    }
}

void AudioVoice::trigger_step(const AccompanimentPlan& plan, int step) noexcept {
    const auto index = static_cast<std::size_t>(step);
    kick_env_ = std::max(kick_env_, plan.kick[index]);
    snare_env_ = std::max(snare_env_, plan.snare[index]);
    hat_env_ = std::max(hat_env_, plan.hat[index]);
    if (plan.bass_midi[index] >= 0) {
        bass_frequency_ = midi_to_hz(plan.bass_midi[index]);
        bass_env_ = 1.0F;
    }
    if (plan.counter_midi[index] >= 0) {
        counter_frequency_ = midi_to_hz(plan.counter_midi[index]);
        counter_env_ = 0.55F;
    }
    if (plan.chord_root_midi[index] >= 0) {
        chord_root_frequency_ = midi_to_hz(plan.chord_root_midi[index]);
        chord_minor_ = plan.chord_minor[index];
    }
}

float AudioVoice::render_compose() noexcept {
    const auto* plan = state_.plan.load(std::memory_order_acquire);
    if (plan == nullptr) return 0.0F;
    const double samples_per_step = static_cast<double>(kSampleRate) * 60.0 / static_cast<double>(std::max(40, plan->bpm)) / 4.0;
    if (step_samples_ <= 0.0) {
        trigger_step(*plan, step_);
        step_samples_ += samples_per_step;
    }
    step_samples_ -= 1.0;
    if (step_samples_ <= 0.0) {
        step_ = (step_ + 1) % kSteps;
        trigger_step(*plan, step_);
    }

    const float bass = triangle(advance(bass_phase_, bass_frequency_)) * bass_env_ * 0.25F;
    bass_env_ *= 0.99945F;
    const float third_ratio = chord_minor_ ? std::pow(2.0F, 3.0F / 12.0F) : std::pow(2.0F, 4.0F / 12.0F);
    const float fifth_ratio = std::pow(2.0F, 7.0F / 12.0F);
    const float pad = (std::sin(2.0F * kPi * advance(pad_phase_a_, chord_root_frequency_)) +
        std::sin(2.0F * kPi * advance(pad_phase_b_, chord_root_frequency_ * third_ratio)) +
        std::sin(2.0F * kPi * advance(pad_phase_c_, chord_root_frequency_ * fifth_ratio))) * 0.045F;
    const float counter = std::sin(2.0F * kPi * advance(counter_phase_, counter_frequency_)) * counter_env_ * 0.12F;
    counter_env_ *= 0.9991F;
    const float kick_frequency = 48.0F + 90.0F * kick_env_;
    const float kick = std::sin(2.0F * kPi * advance(kick_phase_, kick_frequency)) * kick_env_ * 0.72F;
    kick_env_ *= 0.9972F;

    noise_state_ ^= noise_state_ << 13U;
    noise_state_ ^= noise_state_ >> 17U;
    noise_state_ ^= noise_state_ << 5U;
    const float noise = static_cast<float>(static_cast<std::int32_t>(noise_state_)) / static_cast<float>(0x7fffffff);
    const float snare = noise * snare_env_ * 0.25F;
    const float hat = (noise - previous_noise_) * hat_env_ * 0.16F;
    previous_noise_ = noise;
    snare_env_ *= 0.9925F;
    hat_env_ *= 0.965F;
    return bass + pad + counter + kick + snare + hat;
}

float AudioVoice::render_live() noexcept {
    float input = 0.0F;
    state_.live_input.pop(&input, 1U);
    const float agency = state_.agency.load(std::memory_order_relaxed);
    float pitch = state_.pitch_hz.load(std::memory_order_relaxed);
    const float confidence = state_.pitch_confidence.load(std::memory_order_relaxed);
    const bool frozen = state_.frozen.load(std::memory_order_relaxed);
    if (frozen) {
        pitch = state_.frozen_pitch_hz.load(std::memory_order_relaxed);
    } else if (confidence > 0.20F && pitch > 55.0F && pitch < 1200.0F) {
        live_pitch_ += (pitch - live_pitch_) * 0.0009F;
    }
    pitch = clamp(frozen ? pitch : live_pitch_, 45.0F, 880.0F);

    const float rms = state_.rms.load(std::memory_order_relaxed);
    const float target_drone = clamp(rms * 5.0F, 0.0F, 0.46F) * (0.35F + agency * 0.65F);
    drone_env_ += (target_drone - drone_env_) * 0.0005F;
    const float drone = (std::sin(2.0F * kPi * advance(live_phase_a_, pitch * 0.5F)) +
        0.55F * triangle(advance(live_phase_b_, pitch * 0.75F))) * drone_env_ * 0.28F;

    const auto onset = state_.onset_counter.load(std::memory_order_acquire);
    if (onset != seen_onset_) {
        seen_onset_ = onset;
        live_pulse_env_ = 0.72F;
        event_flip_ = !event_flip_;
    }
    const float pulse_frequency = event_flip_ ? 58.0F : 73.0F;
    const float pulse = std::sin(2.0F * kPi * advance(live_pulse_phase_, pulse_frequency)) *
        live_pulse_env_ * (0.18F + agency * 0.22F);
    live_pulse_env_ *= 0.997F;

    const std::size_t write = delay_write_;
    const std::size_t tap_a = (write + delay_.size() - static_cast<std::size_t>(kSampleRate * 0.37)) % delay_.size();
    const std::size_t tap_b = (write + delay_.size() - static_cast<std::size_t>(kSampleRate * 0.83)) % delay_.size();
    const float shadow = delay_[tap_a] * 0.58F + delay_[tap_b] * 0.32F;
    delay_[write] = input + shadow * (0.18F + agency * 0.42F);
    delay_write_ = (write + 1U) % delay_.size();
    return input * 0.10F + drone + pulse + shadow * (0.20F + agency * 0.35F);
}

void capture_callback(void* userdata, Uint8* stream, int length) {
    auto& state = *static_cast<SharedState*>(userdata);
    const auto* samples = reinterpret_cast<const float*>(stream);
    const std::size_t count = static_cast<std::size_t>(length) / sizeof(float);
    state.analysis_input.push(samples, count);
    state.live_input.push(samples, count);
}

void playback_callback(void* userdata, Uint8* stream, int length) {
    auto& voice = *static_cast<AudioVoice*>(userdata);
    auto* output = reinterpret_cast<float*>(stream);
    const int sample_count = length / static_cast<int>(sizeof(float));
    constexpr int channels = 2;
    voice.render(output, sample_count / channels, channels);
}

} // namespace brkbsc::runtime
