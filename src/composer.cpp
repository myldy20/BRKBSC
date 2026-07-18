// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>

namespace brkbsc {
namespace {
float clamp01(float value) noexcept { return std::max(0.0F, std::min(1.0F, value)); }
int positive_mod(int value, int modulus) noexcept { const int result = value % modulus; return result < 0 ? result + modulus : result; }
constexpr std::array<float, 12> kMajorProfile{6.35F, 2.23F, 3.48F, 2.33F, 4.38F, 4.09F, 2.52F, 5.19F, 2.39F, 3.66F, 2.29F, 2.88F};
constexpr std::array<float, 12> kMinorProfile{6.33F, 2.68F, 3.52F, 5.38F, 2.60F, 3.53F, 2.54F, 4.75F, 3.98F, 2.69F, 3.34F, 3.17F};

float profile_score(const std::array<float, 12>& histogram, const std::array<float, 12>& profile, int root) noexcept {
    float score = 0.0F;
    for (int pitch_class = 0; pitch_class < 12; ++pitch_class) {
        score += histogram[static_cast<std::size_t>(pitch_class)] * profile[static_cast<std::size_t>(positive_mod(pitch_class - root, 12))];
    }
    return score;
}

int closest_scale_note(int midi, int root, bool minor) noexcept {
    constexpr std::array<int, 7> major{0, 2, 4, 5, 7, 9, 11};
    constexpr std::array<int, 7> natural_minor{0, 2, 3, 5, 7, 8, 10};
    const auto& scale = minor ? natural_minor : major;
    int best = midi;
    int best_distance = std::numeric_limits<int>::max();
    for (int octave = -1; octave <= 1; ++octave) {
        for (const int degree : scale) {
            const int candidate = ((midi / 12) + octave) * 12 + root + degree;
            const int distance = std::abs(candidate - midi);
            if (distance < best_distance) {
                best = candidate;
                best_distance = distance;
            }
        }
    }
    return best;
}
}

TonalEstimate Composer::estimate_tonality(const std::vector<NoteEvent>& notes) const {
    std::array<float, 12> histogram{};
    for (const auto& note : notes) {
        if (note.midi >= 0) {
            histogram[static_cast<std::size_t>(positive_mod(note.midi, 12))] +=
                std::max(0.125F, note.duration_beats) * (0.35F + note.velocity);
        }
    }
    if (std::accumulate(histogram.begin(), histogram.end(), 0.0F) <= 0.0F) return {};

    float best_score = -std::numeric_limits<float>::infinity();
    float second_score = best_score;
    TonalEstimate result{};
    for (int root = 0; root < 12; ++root) {
        for (int mode = 0; mode < 2; ++mode) {
            const bool minor = mode == 1;
            const float score = profile_score(histogram, minor ? kMinorProfile : kMajorProfile, root);
            if (score > best_score) {
                second_score = best_score;
                best_score = score;
                result.root_pitch_class = root;
                result.minor = minor;
            } else if (score > second_score) {
                second_score = score;
            }
        }
    }
    result.confidence = clamp01((best_score - second_score) / (std::abs(best_score) + 1.0e-6F) * 8.0F);
    return result;
}

AccompanimentPlan Composer::generate(const std::vector<NoteEvent>& notes, int bpm, std::uint32_t seed) const {
    AccompanimentPlan plan{};
    plan.bpm = std::max(40, std::min(240, bpm));
    plan.tonal = estimate_tonality(notes);
    plan.chord_root_midi.fill(-1);
    plan.bass_midi.fill(-1);
    plan.counter_midi.fill(-1);

    std::mt19937 random(seed);
    const int root = plan.tonal.root_pitch_class;
    const bool minor = plan.tonal.minor;
    const std::array<std::array<int, 4>, 2> major_progressions{{{{0, 7, 9, 5}}, {{0, 5, 7, 0}}}};
    const std::array<std::array<int, 4>, 2> minor_progressions{{{{0, 8, 3, 10}}, {{0, 5, 8, 7}}}};
    const auto& progression = minor
        ? minor_progressions[static_cast<std::size_t>(random() % minor_progressions.size())]
        : major_progressions[static_cast<std::size_t>(random() % major_progressions.size())];

    std::array<bool, kSteps> melody_onsets{};
    for (const auto& note : notes) {
        const int step = positive_mod(static_cast<int>(std::lround(note.start_beats * 4.0F)), kSteps);
        melody_onsets[static_cast<std::size_t>(step)] = true;
    }

    for (int bar = 0; bar < 4; ++bar) {
        const int degree = progression[static_cast<std::size_t>(bar)];
        const int chord_root = 48 + positive_mod(root + degree, 12);
        const bool chord_is_minor = minor ? (bar == 0 || degree == 5) : (degree == 9);
        for (int local = 0; local < 4; ++local) {
            const int step = bar * 4 + local;
            plan.chord_root_midi[static_cast<std::size_t>(step)] = chord_root;
            plan.chord_minor[static_cast<std::size_t>(step)] = chord_is_minor;
        }
        const int downbeat = bar * 4;
        plan.bass_midi[static_cast<std::size_t>(downbeat)] = chord_root - 12;
        if (!melody_onsets[static_cast<std::size_t>(downbeat + 2)]) {
            plan.bass_midi[static_cast<std::size_t>(downbeat + 2)] = chord_root - 5;
        }
    }

    plan.kick[0] = 1.0F;
    plan.kick[8] = 0.95F;
    plan.snare[4] = 0.85F;
    plan.snare[12] = 0.90F;
    for (int step = 0; step < kSteps; step += 2) {
        plan.hat[static_cast<std::size_t>(step)] = step % 4 == 0 ? 0.42F : 0.28F;
    }
    for (int step = 0; step < kSteps; ++step) {
        if (melody_onsets[static_cast<std::size_t>(step)] && step != 4 && step != 12) {
            if (step % 2 == 0) {
                plan.kick[static_cast<std::size_t>(step)] = std::max(plan.kick[static_cast<std::size_t>(step)], 0.52F);
            } else {
                plan.hat[static_cast<std::size_t>(step)] = 0.58F;
            }
        }
    }

    for (const auto& note : notes) {
        const int source_step = positive_mod(static_cast<int>(std::lround(note.start_beats * 4.0F)), kSteps);
        const int answer_step = positive_mod(source_step + 2, kSteps);
        if (plan.counter_midi[static_cast<std::size_t>(answer_step)] >= 0) continue;
        int answer = closest_scale_note(note.midi + (minor ? -3 : 4), root, minor);
        while (answer < 60) answer += 12;
        while (answer > 79) answer -= 12;
        plan.counter_midi[static_cast<std::size_t>(answer_step)] = answer;
    }
    return plan;
}

} // namespace brkbsc
