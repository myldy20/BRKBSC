# AI Companion Design

## Product promise

BRKBSC should not behave like an automatic backing-track generator. It should behave like a compact co-composer:

> Give it one musical thought. It returns several genuinely different ways that thought could become a piece, while the musician remains able to keep, reject, freeze, or redirect every role.

The application has two related workflows built on the same musical memory.

## Compose workflow

1. **Listen in silence** — record a hummed or played phrase without the previous arrangement competing with the performer.
2. **Understand** — detect notes, rhythm, phrase shape, tonal ambiguity, repeated motives, and likely cadence points.
3. **Propose** — produce four deliberately different interpretations rather than four random variations:
   - **Anchor** supports the phrase and makes its implied harmony explicit;
   - **Reframe** changes the harmonic meaning through modal mixture, substitutions, pedal tones, or a different tonal centre;
   - **Counter** preserves the phrase but creates rhythmic and melodic opposition around it;
   - **Break** explores a more distant but still traceable interpretation.
4. **Audition** — only one proposal plays at a time.
5. **Pin and mutate** — keep any successful role and regenerate only the others: harmony, bass, rhythm, or response.
6. **Develop** — record another phrase or ask the companion to create a B section using the existing memory.

A musician must never be forced to regenerate the entire result just because one part is weak.

## Live workflow

Live mode should stop following every detected pitch like an audio effect. It should work at phrase and bar boundaries.

1. The performer plays or sings a phrase.
2. BRKBSC identifies a boundary from silence, a button press, or the bar clock.
3. The musical planner prepares the next one or two bars while the current material continues.
4. Enabled roles enter on the next musical boundary:
   - **Ground** — pedal tone, harmonic floor, or sustained texture;
   - **Pulse** — rhythm inferred from attacks but developed rather than copied;
   - **Answer** — a short response derived from the phrase contour;
   - **Shadow** — transformed audio memory used sparingly as texture.
5. While the performer is active, the companion reduces density. During space, it may become more assertive.

This creates call-and-response instead of pitch-tracking delay.

## Steering without genre presets

The main steering surface is an **Intent Compass**, not a menu of moods or genres.

### Two-dimensional compass

- horizontal: **Faithful ↔ Contrary** — how strongly the result follows the input;
- vertical: **Still ↔ Driving** — how much rhythmic and structural motion it creates.

### Secondary constraints

- **Tension** — harmonic instability, dissonance, delayed resolution, and rhythmic friction;
- **Density** — number of active layers and events;
- **Evolution** — stable loop versus gradual structural change;
- **Range** — compact voicing versus wide register;
- **Tempo** — explicit, tapped, or inferred.

These controls must condition musical decisions, not merely change synthesis parameters after generation.

## Musical intelligence architecture

BRKBSC should use a hybrid pipeline.

### 1. Deterministic listening front end

- pitch and onset detection;
- phrase segmentation;
- tempo and meter hypotheses;
- note confidence and octave correction;
- motif and contour extraction.

This layer stays transparent, fast, and debuggable.

### 2. Harmonic planner

A small learned symbolic model receives the melody plus intent values and proposes chord functions, bass targets, cadence points, and harmonic rhythm. It should produce several ranked plans rather than one answer.

The planner is the first place where learned behaviour can create a major musical improvement. The current hard-coded four-chord tables cannot express ambiguity, reharmonisation, voice-leading, or long-term direction.

### 3. Role generators

Separate lightweight generators create:

- expressive drums and fills;
- bass motion consistent with the harmonic plan;
- counter-melody and response phrases;
- arrangement density and entry/exit decisions.

They may share one compact event model, but each role remains independently replaceable and mutable in the product.

### 4. Native DSP renderer

The neural model outputs symbolic events and control trajectories. C++ DSP produces the audio. This keeps inference outside the real-time audio callback and avoids asking the model to synthesize 48,000 waveform samples every second.

## Proposed model target

The first learned prototype should be a compact causal GRU or Transformer trained off-device and exported as an int8 model.

Initial engineering budget:

- 1–5 million parameters;
- symbolic event rate, not waveform rate;
- generation one or two bars ahead;
- inference in a worker thread;
- hard fallback to deterministic rules;
- measurable CPU and latency limits on TrimUI Brick.

Existing research demonstrates that symbolic models can support real-time harmonisation and improvisation with low latency. BRKBSC should borrow the interaction principles, not attempt to embed a large general-purpose model unchanged.

## Training strategy

Use legally reusable symbolic datasets and synthetic augmentation:

- Groove MIDI Dataset for expressive drum timing and velocity;
- public-domain and permissively licensed MIDI for melody, harmony, and bass relationships;
- transposition, rhythmic displacement, reharmonisation, role masking, and controlled corruption;
- generated negative examples to teach clash avoidance and register separation.

The model should be evaluated by blind musical preference tests, not only token accuracy.

## Development order

1. Fix capture silence and phrase restart behaviour.
2. Replace one fixed accompaniment with four deterministic proposal types.
3. Add role pinning, muting, and role-only regeneration.
4. Add the Intent Compass and verify that its extremes sound meaningfully different.
5. Build a desktop Python training and evaluation pipeline.
6. Train the harmonic planner first.
7. Export a quantized model and profile it on Brick.
8. Add learned drums, bass, and responses only after the harmonic planner is clearly better than rules.

## Product test

The central test is not whether the output sounds polished. It is:

> Does BRKBSC make the musician hear a promising direction they would not have reached immediately, while still feeling that the original idea belongs to them?

If it only decorates the input, it is an effect. If it ignores the input, it is a generator. BRKBSC succeeds only in the space between those two failures.

## Research references

- Notochord: A Flexible Probabilistic Model for Real-Time MIDI Performance, 2024.
- SongDriver: Real-time Music Accompaniment Generation without Logical Latency nor Exposure Bias, 2022.
- Groove MIDI Dataset and GrooVAE, Magenta.
- LiveBand: Live Accompaniment Generation in the Audio Domain, 2026 — relevant long-term research, but too heavy for the initial Brick architecture.
