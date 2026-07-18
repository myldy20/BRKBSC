// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace brkbsc {
namespace {
float clamp01(float value) noexcept { return std::max(0.0F, std::min(1.0F, value)); }
int positive_mod(int value, int modulus) noexcept { const int result = value % modulus; return result < 0 ? result + modulus : result; }
}

float midi_to_hz(int midi) noexcept {
    return 440.0F * std::pow(2.0F, static_cast<float>(midi - 69) / 12.0F);
}

int hz_to_midi(float hz) noexcept {
    if (!(hz > 0.0F)) return -1;
    return static_cast<int>(std::lround(69.0F + 12.0F * std::log2(hz / 440.0F)));
}

const char* pitch_class_name(int pitch_class) noexcept {
    constexpr std::array<const char*, 12> names{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return names[static_cast<std::size_t>(positive_mod(pitch_class, 12))];
}

AnalysisFrame PitchDetector::analyse(const float* samples, std::size_t count, int sample_rate) {
    AnalysisFrame frame{};
    if (samples == nullptr || count < 256U || sample_rate <= 0) return frame;

    double energy = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        const double sample = static_cast<double>(samples[index]);
        energy += sample * sample;
    }
    frame.rms = static_cast<float>(std::sqrt(energy / static_cast<double>(count)));
    frame.onset = frame.rms > 0.018F && frame.rms > previous_rms_ * 1.55F;
    previous_rms_ = 0.82F * previous_rms_ + 0.18F * frame.rms;
    if (frame.rms < 0.009F) return frame;

    constexpr int decimation = 4;
    const std::size_t reduced_count = count / static_cast<std::size_t>(decimation);
    const int reduced_rate = sample_rate / decimation;
    const int minimum_lag = std::max(2, reduced_rate / 1000);
    const int maximum_lag = std::min(static_cast<int>(reduced_count / 2U), reduced_rate / 65);

    float best_correlation = 0.0F;
    int best_lag = 0;
    std::vector<float> correlations(static_cast<std::size_t>(maximum_lag + 1), 0.0F);
    for (int lag = minimum_lag; lag <= maximum_lag; ++lag) {
        double numerator = 0.0;
        double energy_a = 0.0;
        double energy_b = 0.0;
        const std::size_t usable = reduced_count - static_cast<std::size_t>(lag);
        for (std::size_t index = 0; index < usable; ++index) {
            const float a = samples[index * static_cast<std::size_t>(decimation)];
            const float b = samples[(index + static_cast<std::size_t>(lag)) * static_cast<std::size_t>(decimation)];
            numerator += static_cast<double>(a) * static_cast<double>(b);
            energy_a += static_cast<double>(a) * static_cast<double>(a);
            energy_b += static_cast<double>(b) * static_cast<double>(b);
        }
        const float correlation = static_cast<float>(numerator / (std::sqrt(energy_a * energy_b) + 1.0e-12));
        correlations[static_cast<std::size_t>(lag)] = correlation;
        if (correlation > best_correlation) {
            best_correlation = correlation;
            best_lag = lag;
        }
    }

    if (best_lag > 0 && best_correlation > 0.58F) {
        const float strong_peak = std::max(0.62F, best_correlation * 0.92F);
        for (int lag = minimum_lag + 1; lag < maximum_lag; ++lag) {
            const float current = correlations[static_cast<std::size_t>(lag)];
            if (current >= strong_peak && current >= correlations[static_cast<std::size_t>(lag - 1)] && current >= correlations[static_cast<std::size_t>(lag + 1)]) {
                best_lag = lag;
                best_correlation = current;
                break;
            }
        }
        frame.pitch_hz = static_cast<float>(reduced_rate) / static_cast<float>(best_lag);
        frame.pitch_confidence = clamp01((best_correlation - 0.50F) / 0.45F);
    }
    return frame;
}

} // namespace brkbsc
