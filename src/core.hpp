// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace brkbsc {

constexpr int kSteps = 32;
constexpr int kSampleRate = 48000;
constexpr int kChordVoices = 4;

enum class ProposalKind : std::uint8_t { Anchor = 0, Reframe = 1, Counter = 2, Break = 3 };
enum class Role : std::uint8_t { Harmony = 0, Bass = 1, Rhythm = 2, Answer = 3 };

struct Intent {
    float contrary = 0.25F;
    float drive = 0.45F;
    float tension = 0.35F;
    float density = 0.50F;
    float evolution = 0.35F;
};

struct AnalysisFrame {
    float rms = 0.0F;
    float pitch_hz = 0.0F;
    float pitch_confidence = 0.0F;
    bool onset = false;
};

struct NoteEvent {
    int midi = -1;
    float start_beats = 0.0F;
    float duration_beats = 0.0F;
    float velocity = 0.0F;
};

struct TonalEstimate {
    int root_pitch_class = 0;
    bool minor = false;
    float confidence = 0.0F;
};

struct AccompanimentPlan {
    int bpm = 90;
    ProposalKind kind = ProposalKind::Anchor;
    Intent intent{};
    TonalEstimate tonal{};
    std::array<std::array<int, kChordVoices>, kSteps> chord_midi{};
    std::array<int, kSteps> bass_midi{};
    std::array<int, kSteps> counter_midi{};
    std::array<float, kSteps> kick{};
    std::array<float, kSteps> snare{};
    std::array<float, kSteps> hat{};
};

class PitchDetector {
public:
    AnalysisFrame analyse(const float* samples, std::size_t count, int sample_rate);
private:
    float previous_rms_ = 0.0F;
};

class PhraseRecorder {
public:
    void start(double time_seconds, int bpm);
    void stop(double time_seconds);
    void clear();
    void process(const AnalysisFrame& frame, double time_seconds);
    [[nodiscard]] bool recording() const noexcept { return recording_; }
    [[nodiscard]] const std::vector<NoteEvent>& notes() const noexcept { return notes_; }
private:
    void finish_current(double time_seconds);
    bool recording_ = false;
    int bpm_ = 90;
    double started_at_ = 0.0;
    double current_started_at_ = 0.0;
    int current_midi_ = -1;
    float current_velocity_ = 0.0F;
    std::vector<NoteEvent> notes_{};
};

class Composer {
public:
    [[nodiscard]] TonalEstimate estimate_tonality(const std::vector<NoteEvent>& notes) const;
    [[nodiscard]] AccompanimentPlan generate(
        const std::vector<NoteEvent>& notes,
        int bpm,
        std::uint32_t seed,
        ProposalKind kind = ProposalKind::Anchor,
        Intent intent = {}) const;
};

[[nodiscard]] float midi_to_hz(int midi) noexcept;
[[nodiscard]] int hz_to_midi(float hz) noexcept;
[[nodiscard]] const char* pitch_class_name(int pitch_class) noexcept;
[[nodiscard]] const char* proposal_name(ProposalKind kind) noexcept;
[[nodiscard]] const char* role_name(Role role) noexcept;

} // namespace brkbsc
