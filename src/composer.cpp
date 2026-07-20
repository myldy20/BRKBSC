// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

namespace brkbsc {
namespace {

float clamp01(float value) noexcept { return std::max(0.0F, std::min(1.0F, value)); }
int positive_mod(int value, int modulus) noexcept { const int result = value % modulus; return result < 0 ? result + modulus : result; }

constexpr std::array<float, 12> kMajorProfile{6.35F, 2.23F, 3.48F, 2.33F, 4.38F, 4.09F, 2.52F, 5.19F, 2.39F, 3.66F, 2.29F, 2.88F};
constexpr std::array<float, 12> kMinorProfile{6.33F, 2.68F, 3.52F, 5.38F, 2.60F, 3.53F, 2.54F, 4.75F, 3.98F, 2.69F, 3.34F, 3.17F};
constexpr std::array<int, 7> kMajorScale{0, 2, 4, 5, 7, 9, 11};
constexpr std::array<int, 7> kMinorScale{0, 2, 3, 5, 7, 8, 10};

struct ChordCandidate {
    int root_pc = 0;
    std::array<int, kChordVoices> intervals{0, 4, 7, 11};
    float colour = 0.0F;
    float stability = 1.0F;
};

float profile_score(const std::array<float, 12>& histogram, const std::array<float, 12>& profile, int root) noexcept {
    float score = 0.0F;
    for (int pitch_class = 0; pitch_class < 12; ++pitch_class) {
        score += histogram[static_cast<std::size_t>(pitch_class)] * profile[static_cast<std::size_t>(positive_mod(pitch_class - root, 12))];
    }
    return score;
}

bool contains_pc(const ChordCandidate& chord, int pitch_class) noexcept {
    for (const int interval : chord.intervals) {
        if (positive_mod(chord.root_pc + interval, 12) == pitch_class) return true;
    }
    return false;
}

bool in_scale(int pitch_class, int root, bool minor) noexcept {
    const auto& scale = minor ? kMinorScale : kMajorScale;
    const int relative = positive_mod(pitch_class - root, 12);
    return std::find(scale.begin(), scale.end(), relative) != scale.end();
}

int closest_scale_note(int midi, int root, bool minor) noexcept {
    const auto& scale = minor ? kMinorScale : kMajorScale;
    int best = midi;
    int best_distance = std::numeric_limits<int>::max();
    for (int octave = -2; octave <= 2; ++octave) {
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

std::array<int, kChordVoices> seventh_intervals(bool minor, int degree) noexcept {
    if (!minor) {
        constexpr std::array<std::array<int, kChordVoices>, 7> qualities{{
            {{0, 4, 7, 11}}, {{0, 3, 7, 10}}, {{0, 3, 7, 10}}, {{0, 4, 7, 11}},
            {{0, 4, 7, 10}}, {{0, 3, 7, 10}}, {{0, 3, 6, 10}},
        }};
        return qualities[static_cast<std::size_t>(degree)];
    }
    constexpr std::array<std::array<int, kChordVoices>, 7> qualities{{
        {{0, 3, 7, 10}}, {{0, 3, 6, 10}}, {{0, 4, 7, 11}}, {{0, 3, 7, 10}},
        {{0, 3, 7, 10}}, {{0, 4, 7, 11}}, {{0, 4, 7, 10}},
    }};
    return qualities[static_cast<std::size_t>(degree)];
}

std::vector<ChordCandidate> build_palette(const TonalEstimate& tonal, ProposalKind kind, const Intent& intent) {
    std::vector<ChordCandidate> palette;
    const auto& scale = tonal.minor ? kMinorScale : kMajorScale;
    for (int degree = 0; degree < 7; ++degree) {
        palette.push_back(ChordCandidate{
            positive_mod(tonal.root_pitch_class + scale[static_cast<std::size_t>(degree)], 12),
            seventh_intervals(tonal.minor, degree),
            degree == 0 ? 0.0F : 0.12F + 0.05F * static_cast<float>(degree),
            degree == 0 || degree == 3 || degree == 4 ? 1.0F : 0.78F,
        });
    }

    const float contrary = clamp01(intent.contrary);
    const float tension = clamp01(intent.tension);
    if (kind != ProposalKind::Anchor || contrary > 0.35F) {
        palette.push_back({positive_mod(tonal.root_pitch_class + 1, 12), {{0, 4, 7, 11}}, 0.70F, 0.45F});
        palette.push_back({positive_mod(tonal.root_pitch_class + 8, 12), {{0, 4, 7, 11}}, 0.48F, 0.68F});
        palette.push_back({positive_mod(tonal.root_pitch_class + 5, 12), {{0, 3, 7, 10}}, 0.38F, 0.72F});
    }
    if (kind == ProposalKind::Counter || kind == ProposalKind::Break || tension > 0.62F) {
        palette.push_back({positive_mod(tonal.root_pitch_class + 2, 12), {{0, 4, 7, 10}}, 0.62F, 0.55F});
        palette.push_back({positive_mod(tonal.root_pitch_class + 6, 12), {{0, 3, 6, 9}}, 0.95F, 0.25F});
        palette.push_back({positive_mod(tonal.root_pitch_class + 10, 12), {{0, 5, 7, 10}}, 0.58F, 0.52F});
    }
    return palette;
}

std::array<std::array<float, 12>, 4> segment_histograms(const std::vector<NoteEvent>& notes) {
    std::array<std::array<float, 12>, 4> result{};
    for (const auto& note : notes) {
        if (note.midi < 0) continue;
        const int step = positive_mod(static_cast<int>(std::lround(note.start_beats * 4.0F)), kSteps);
        const int segment = std::min(3, step / 8);
        result[static_cast<std::size_t>(segment)][static_cast<std::size_t>(positive_mod(note.midi, 12))] +=
            std::max(0.25F, note.duration_beats) * (0.4F + note.velocity);
    }
    return result;
}

float candidate_score(
    const ChordCandidate& chord,
    const std::array<float, 12>& melody,
    const TonalEstimate& tonal,
    ProposalKind kind,
    const Intent& intent,
    int previous_root,
    int segment) noexcept {
    float score = 0.0F;
    float melody_weight = 0.0F;
    for (int pc = 0; pc < 12; ++pc) {
        const float weight = melody[static_cast<std::size_t>(pc)];
        melody_weight += weight;
        if (contains_pc(chord, pc)) score += weight * (kind == ProposalKind::Reframe ? 2.4F : 4.2F);
        else if (in_scale(pc, tonal.root_pitch_class, tonal.minor)) score += weight * 0.45F;
        else score -= weight * (0.8F - 0.45F * clamp01(intent.tension));
    }
    if (melody_weight <= 0.0F) score += chord.stability * 1.8F;

    const float contrary = clamp01(intent.contrary);
    const float tension = clamp01(intent.tension);
    if (kind == ProposalKind::Anchor) score += chord.stability * (3.4F - 1.8F * contrary) - chord.colour * 1.2F;
    if (kind == ProposalKind::Reframe) score += chord.colour * (2.0F + contrary) + chord.stability * 0.6F;
    if (kind == ProposalKind::Counter) score += chord.colour * 1.2F + (segment % 2 == 1 ? 0.8F : 0.0F);
    if (kind == ProposalKind::Break) score += chord.colour * (2.4F + 1.8F * tension) - chord.stability * 0.25F;

    if (previous_root >= 0) {
        const int distance = std::min(positive_mod(chord.root_pc - previous_root, 12), positive_mod(previous_root - chord.root_pc, 12));
        const float smoothness = kind == ProposalKind::Break ? 0.08F : 0.20F + (1.0F - contrary) * 0.25F;
        score -= static_cast<float>(distance) * smoothness;
        if (kind == ProposalKind::Counter && distance >= 4 && distance <= 7) score += 0.7F;
    }
    return score;
}

std::array<int, kChordVoices> voice_chord(const ChordCandidate& chord, const std::array<int, kChordVoices>& previous) noexcept {
    std::array<int, kChordVoices> voicing{};
    int root = 48 + chord.root_pc;
    while (root < 48) root += 12;
    while (root > 59) root -= 12;
    for (int voice = 0; voice < kChordVoices; ++voice) voicing[static_cast<std::size_t>(voice)] = root + chord.intervals[static_cast<std::size_t>(voice)];

    if (previous[0] >= 0) {
        for (int voice = 0; voice < kChordVoices; ++voice) {
            int best = voicing[static_cast<std::size_t>(voice)];
            int best_distance = std::abs(best - previous[static_cast<std::size_t>(voice)]);
            for (int octave = -2; octave <= 2; ++octave) {
                const int candidate = voicing[static_cast<std::size_t>(voice)] + octave * 12;
                if (candidate < 43 || candidate > 81) continue;
                const int distance = std::abs(candidate - previous[static_cast<std::size_t>(voice)]);
                if (distance < best_distance) { best = candidate; best_distance = distance; }
            }
            voicing[static_cast<std::size_t>(voice)] = best;
        }
        std::sort(voicing.begin(), voicing.end());
    }
    return voicing;
}

std::array<int, 4> choose_progression(
    const std::vector<ChordCandidate>& palette,
    const std::array<std::array<float, 12>, 4>& histograms,
    const TonalEstimate& tonal,
    ProposalKind kind,
    const Intent& intent) {
    constexpr float kNegative = -1.0e20F;
    std::vector<std::array<float, 4>> scores(palette.size());
    std::vector<std::array<int, 4>> parents(palette.size());
    for (auto& row : scores) row.fill(kNegative);
    for (auto& row : parents) row.fill(-1);

    for (std::size_t candidate = 0; candidate < palette.size(); ++candidate) {
        scores[candidate][0] = candidate_score(palette[candidate], histograms[0], tonal, kind, intent, -1, 0);
    }
    for (int segment = 1; segment < 4; ++segment) {
        for (std::size_t candidate = 0; candidate < palette.size(); ++candidate) {
            for (std::size_t previous = 0; previous < palette.size(); ++previous) {
                const float value = scores[previous][static_cast<std::size_t>(segment - 1)] + candidate_score(
                    palette[candidate], histograms[static_cast<std::size_t>(segment)], tonal, kind, intent,
                    palette[previous].root_pc, segment);
                if (value > scores[candidate][static_cast<std::size_t>(segment)]) {
                    scores[candidate][static_cast<std::size_t>(segment)] = value;
                    parents[candidate][static_cast<std::size_t>(segment)] = static_cast<int>(previous);
                }
            }
        }
    }

    int best = 0;
    for (std::size_t candidate = 1; candidate < palette.size(); ++candidate) {
        if (scores[candidate][3] > scores[static_cast<std::size_t>(best)][3]) best = static_cast<int>(candidate);
    }
    std::array<int, 4> progression{};
    for (int segment = 3; segment >= 0; --segment) {
        progression[static_cast<std::size_t>(segment)] = best;
        best = segment > 0 ? parents[static_cast<std::size_t>(best)][static_cast<std::size_t>(segment)] : best;
    }
    return progression;
}

void write_rhythm(AccompanimentPlan& plan, std::mt19937& random) {
    const float drive = clamp01(plan.intent.drive);
    const float density = clamp01(plan.intent.density);
    const float evolution = clamp01(plan.intent.evolution);

    for (int step = 0; step < kSteps; step += 2) {
        const float base = step % 4 == 0 ? 0.36F : 0.22F;
        if (density > 0.28F || step % 4 == 0) plan.hat[static_cast<std::size_t>(step)] = base + density * 0.20F;
    }

    if (plan.kind == ProposalKind::Anchor) {
        plan.kick[0] = 1.0F; plan.kick[16] = 0.95F;
        plan.snare[8] = 0.82F; plan.snare[24] = 0.88F;
        if (drive > 0.48F) { plan.kick[10] = 0.48F; plan.kick[26] = 0.55F; }
    } else if (plan.kind == ProposalKind::Reframe) {
        plan.kick[0] = 0.90F; plan.kick[18] = 0.72F;
        plan.snare[12] = 0.82F; plan.snare[28] = 0.90F;
        if (drive > 0.55F) { plan.hat[7] = 0.52F; plan.hat[15] = 0.58F; plan.hat[23] = 0.52F; plan.hat[31] = 0.65F; }
    } else if (plan.kind == ProposalKind::Counter) {
        plan.kick[0] = 0.92F; plan.kick[6] = 0.55F; plan.kick[17] = 0.86F; plan.kick[22] = 0.62F;
        plan.snare[8] = 0.80F; plan.snare[20] = 0.60F; plan.snare[24] = 0.90F;
        for (int step : {3, 11, 19, 27}) plan.hat[static_cast<std::size_t>(step)] = 0.48F + drive * 0.18F;
    } else {
        const int offset = static_cast<int>(random() % 5U);
        for (int step = offset; step < kSteps; step += drive > 0.55F ? 5 : 7) plan.kick[static_cast<std::size_t>(step)] = 0.58F + 0.34F * drive;
        for (int step = 8; step < kSteps; step += evolution > 0.5F ? 11 : 16) plan.snare[static_cast<std::size_t>(step)] = 0.68F + 0.22F * density;
        for (int step = 1; step < kSteps; step += 3) if ((random() & 3U) != 0U) plan.hat[static_cast<std::size_t>(step)] = 0.28F + density * 0.30F;
    }
}

void write_bass(AccompanimentPlan& plan, const std::array<int, 4>& roots, std::mt19937& random) {
    const float drive = clamp01(plan.intent.drive);
    const float density = clamp01(plan.intent.density);
    int previous = 36 + roots[0];
    for (int segment = 0; segment < 4; ++segment) {
        const int start = segment * 8;
        int root = 36 + roots[static_cast<std::size_t>(segment)];
        while (root - previous > 6) root -= 12;
        while (previous - root > 6) root += 12;
        if (plan.kind == ProposalKind::Counter) {
            const int direction = segment == 0 ? 0 : (root >= previous ? -1 : 1);
            root = closest_scale_note(root + direction * (drive > 0.5F ? 5 : 3), plan.tonal.root_pitch_class, plan.tonal.minor);
        }
        plan.bass_midi[static_cast<std::size_t>(start)] = root;
        if (drive > 0.30F) plan.bass_midi[static_cast<std::size_t>(start + 4)] = plan.kind == ProposalKind::Reframe ? root + 7 : root + 12;
        if (density > 0.58F) plan.bass_midi[static_cast<std::size_t>(start + 6)] = closest_scale_note(root + ((random() & 1U) != 0U ? 2 : -2), plan.tonal.root_pitch_class, plan.tonal.minor);
        if (plan.kind == ProposalKind::Break && segment % 2 == 1) plan.bass_midi[static_cast<std::size_t>(start + 3)] = root + 1;
        previous = root;
    }
}

void write_answer(AccompanimentPlan& plan, const std::vector<NoteEvent>& notes) {
    if (notes.empty()) return;
    const float density = clamp01(plan.intent.density);
    const int limit = density > 0.68F ? 10 : density > 0.38F ? 7 : 4;
    int written = 0;
    for (std::size_t source_index = 0; source_index < notes.size() && written < limit; ++source_index) {
        const auto& source = plan.kind == ProposalKind::Reframe
            ? notes[notes.size() - 1U - source_index]
            : notes[source_index];
        if (source.midi < 0) continue;
        const int source_step = positive_mod(static_cast<int>(std::lround(source.start_beats * 4.0F)), kSteps);
        const int answer_step = positive_mod(source_step + (plan.kind == ProposalKind::Anchor ? 4 : plan.kind == ProposalKind::Counter ? 6 : 8), kSteps);
        if (plan.counter_midi[static_cast<std::size_t>(answer_step)] >= 0) continue;
        int target = source.midi;
        if (plan.kind == ProposalKind::Anchor) target += plan.tonal.minor ? 3 : 4;
        else if (plan.kind == ProposalKind::Reframe) target += plan.tonal.minor ? -5 : -4;
        else if (plan.kind == ProposalKind::Counter) {
            const int tonic = 60 + plan.tonal.root_pitch_class;
            target = tonic - (source.midi - tonic);
        } else target += source_index % 2U == 0U ? 6 : -1;
        target = closest_scale_note(target, plan.tonal.root_pitch_class, plan.tonal.minor);
        while (target < 60) target += 12;
        while (target > 84) target -= 12;
        plan.counter_midi[static_cast<std::size_t>(answer_step)] = target;
        ++written;
    }
}

} // namespace

const char* proposal_name(ProposalKind kind) noexcept {
    switch (kind) {
    case ProposalKind::Anchor: return "ANCHOR";
    case ProposalKind::Reframe: return "REFRAME";
    case ProposalKind::Counter: return "COUNTER";
    case ProposalKind::Break: return "BREAK";
    }
    return "ANCHOR";
}

const char* role_name(Role role) noexcept {
    switch (role) {
    case Role::Harmony: return "HARMONY";
    case Role::Bass: return "BASS";
    case Role::Rhythm: return "RHYTHM";
    case Role::Answer: return "ANSWER";
    }
    return "HARMONY";
}

TonalEstimate Composer::estimate_tonality(const std::vector<NoteEvent>& notes) const {
    std::array<float, 12> histogram{};
    for (const auto& note : notes) {
        if (note.midi >= 0) histogram[static_cast<std::size_t>(positive_mod(note.midi, 12))] += std::max(0.125F, note.duration_beats) * (0.35F + note.velocity);
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
            } else if (score > second_score) second_score = score;
        }
    }
    result.confidence = clamp01((best_score - second_score) / (std::abs(best_score) + 1.0e-6F) * 8.0F);
    return result;
}

AccompanimentPlan Composer::generate(const std::vector<NoteEvent>& notes, int bpm, std::uint32_t seed, ProposalKind kind, Intent intent) const {
    AccompanimentPlan plan{};
    plan.bpm = std::max(40, std::min(240, bpm));
    plan.kind = kind;
    plan.intent = intent;
    plan.tonal = estimate_tonality(notes);
    for (auto& chord : plan.chord_midi) chord.fill(-1);
    plan.bass_midi.fill(-1);
    plan.counter_midi.fill(-1);

    std::mt19937 random(seed ^ (0x9e3779b9U * (1U + static_cast<std::uint32_t>(kind))));
    const auto palette = build_palette(plan.tonal, kind, intent);
    const auto histograms = segment_histograms(notes);
    const auto progression = choose_progression(palette, histograms, plan.tonal, kind, intent);

    std::array<int, kChordVoices> previous{};
    previous.fill(-1);
    std::array<int, 4> roots{};
    for (int segment = 0; segment < 4; ++segment) {
        const auto& candidate = palette[static_cast<std::size_t>(progression[static_cast<std::size_t>(segment)])];
        roots[static_cast<std::size_t>(segment)] = candidate.root_pc;
        auto voicing = voice_chord(candidate, previous);
        if (kind == ProposalKind::Break && segment == 3 && intent.evolution > 0.45F) voicing[3] += 1;
        for (int local = 0; local < 8; ++local) plan.chord_midi[static_cast<std::size_t>(segment * 8 + local)] = voicing;
        previous = voicing;
    }

    write_bass(plan, roots, random);
    write_rhythm(plan, random);
    write_answer(plan, notes);
    return plan;
}

} // namespace brkbsc
