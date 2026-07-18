// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.hpp"

#include <algorithm>
#include <cmath>

namespace brkbsc {
namespace {
float clamp01(float value) noexcept { return std::max(0.0F, std::min(1.0F, value)); }
float beats_from_seconds(double seconds, int bpm) noexcept { return static_cast<float>(seconds * static_cast<double>(bpm) / 60.0); }
}

void PhraseRecorder::start(double time_seconds, int bpm) {
    clear();
    recording_ = true;
    bpm_ = std::max(40, std::min(240, bpm));
    started_at_ = time_seconds;
    current_started_at_ = time_seconds;
}

void PhraseRecorder::stop(double time_seconds) {
    if (!recording_) return;
    finish_current(time_seconds);
    recording_ = false;
}

void PhraseRecorder::clear() {
    recording_ = false;
    current_midi_ = -1;
    current_velocity_ = 0.0F;
    notes_.clear();
}

void PhraseRecorder::finish_current(double time_seconds) {
    if (current_midi_ < 0) return;
    notes_.push_back(NoteEvent{
        current_midi_,
        beats_from_seconds(current_started_at_ - started_at_, bpm_),
        std::max(0.125F, beats_from_seconds(time_seconds - current_started_at_, bpm_)),
        current_velocity_,
    });
    current_midi_ = -1;
    current_velocity_ = 0.0F;
}

void PhraseRecorder::process(const AnalysisFrame& frame, double time_seconds) {
    if (!recording_) return;
    const bool voiced = frame.pitch_confidence >= 0.25F && frame.pitch_hz >= 65.0F;
    const int midi = voiced ? hz_to_midi(frame.pitch_hz) : -1;
    const bool changed = current_midi_ >= 0 && midi >= 0 && std::abs(midi - current_midi_) >= 1;
    const bool silence = !voiced && frame.rms < 0.012F;

    if (current_midi_ >= 0 && (changed || silence || frame.onset)) finish_current(time_seconds);

    if (midi >= 0 && current_midi_ < 0) {
        current_midi_ = midi;
        current_started_at_ = time_seconds;
        current_velocity_ = clamp01(frame.rms * 8.0F);
    } else if (midi >= 0 && current_midi_ >= 0) {
        current_velocity_ = std::max(current_velocity_, clamp01(frame.rms * 8.0F));
    }
}

} // namespace brkbsc
