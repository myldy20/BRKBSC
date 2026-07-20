// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

namespace {
std::string fingerprint(const brkbsc::AccompanimentPlan& plan) {
    std::ostringstream out;
    for (int segment = 0; segment < 4; ++segment) {
        const auto& chord = plan.chord_midi[static_cast<std::size_t>(segment * 8)];
        for (int note : chord) out << note << ',';
        out << '|';
    }
    for (int note : plan.bass_midi) if (note >= 0) out << 'b' << note << ',';
    return out.str();
}
}

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
        {60, 0.0F, 0.75F, 0.8F}, {64, 0.75F, 0.75F, 0.8F}, {67, 1.5F, 1.0F, 0.9F},
        {69, 2.5F, 0.5F, 0.7F}, {67, 3.0F, 1.0F, 0.8F}, {62, 4.0F, 1.0F, 0.8F},
        {65, 5.0F, 1.0F, 0.8F}, {64, 6.0F, 1.5F, 0.9F},
    };
    Composer composer;
    const auto tonal = composer.estimate_tonality(melody);
    assert(tonal.confidence >= 0.0F);

    Intent intent{};
    intent.drive = 0.7F;
    intent.tension = 0.55F;
    std::set<std::string> fingerprints;
    for (int kind = 0; kind < 4; ++kind) {
        const auto plan = composer.generate(melody, 104, 42U, static_cast<ProposalKind>(kind), intent);
        assert(plan.bpm == 104);
        assert(plan.chord_midi[0][0] >= 0);
        assert(plan.chord_midi[24][3] >= 0);
        assert(plan.bass_midi[0] >= 0);
        assert(plan.kick[0] > 0.0F || kind == static_cast<int>(ProposalKind::Break));
        fingerprints.insert(fingerprint(plan));
        const auto duplicate = composer.generate(melody, 104, 42U, static_cast<ProposalKind>(kind), intent);
        assert(fingerprint(plan) == fingerprint(duplicate));
    }
    assert(fingerprints.size() == 4U);
    assert(std::string(proposal_name(ProposalKind::Reframe)) == "REFRAME");
    assert(std::string(role_name(Role::Answer)) == "ANSWER");

    std::cout << "BRKBSC core tests passed\n";
    return 0;
}
