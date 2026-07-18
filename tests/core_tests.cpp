// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    using namespace brkbsc;

    assert(std::abs(midi_to_hz(69) - 440.0F) < 0.01F);
    assert(hz_to_midi(440.0F) == 69);

    std::vector<float> sine(2048U);
    for (std::size_t index = 0; index < sine.size(); ++index) {
        sine[index] = 0.2F * std::sin(
            2.0F * 3.14159265358979323846F * 220.0F *
            static_cast<float>(index) / static_cast<float>(kSampleRate));
    }
    PitchDetector detector;
    const auto frame = detector.analyse(sine.data(), sine.size(), kSampleRate);
    assert(frame.pitch_confidence > 0.2F);
    assert(std::abs(frame.pitch_hz - 220.0F) < 12.0F);

    const std::vector<NoteEvent> melody{
        {60, 0.0F, 1.0F, 0.8F},
        {64, 1.0F, 1.0F, 0.8F},
        {67, 2.0F, 1.5F, 0.9F},
        {72, 3.5F, 1.0F, 0.8F},
    };
    Composer composer;
    const auto tonal = composer.estimate_tonality(melody);
    assert(tonal.root_pitch_class == 0);
    assert(!tonal.minor);

    const auto plan = composer.generate(melody, 92, 42U);
    assert(plan.bpm == 92);
    assert(plan.bass_midi[0] >= 0);
    assert(plan.kick[0] > 0.0F);
    assert(plan.snare[4] > 0.0F);

    std::cout << "BRKBSC core tests passed\n";
    return 0;
}
