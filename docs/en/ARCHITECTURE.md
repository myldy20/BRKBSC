# Architecture

## Target constraints

Primary target: TrimUI Brick, 1024×768 display, ARM Cortex-A53 class CPU, approximately 1 GB RAM, no neural accelerator assumed. Desktop macOS/Linux builds are the development environment.

The architecture protects the audio callback from expensive or unpredictable work. Analysis and future inference run outside the playback callback. If analysis stalls, audio continues with the last stable state.

## Data flow

```text
Microphone capture callback
       |                 \
       v                  v
analysis ring          live-audio ring
       |                  |
main-thread analysis     playback callback
       |                  |
pitch / rms / onset      monitor + shadow delay
       |
phrase recorder or live control state
       |
composer / future tiny model
       |
immutable accompaniment plan
       |
playback callback -> synth voices -> output
```

## Current modules

### `PitchDetector`

A lightweight normalized autocorrelation detector. Input is decimated by four before correlation to reduce CPU use. The detector returns RMS level, onset flag, monophonic pitch, and confidence. Low-confidence frames produce no new note rather than unstable pitch output.

### `PhraseRecorder`

Converts analysis frames into note events. A note ends when a sufficiently different pitch appears, silence is detected, a new onset separates repeated notes, or recording stops. Events are stored in beats using the current manual BPM.

### `Composer`

The alpha uses deterministic music rules:

1. build a duration- and velocity-weighted pitch-class histogram;
2. score 12 major and 12 minor tonal profiles;
3. select one of a few compatible four-chord progressions;
4. place bass roots and fifths;
5. derive drum accents from recorded onsets;
6. create a short scale-constrained answering motif.

A seed changes the interpretation while preserving the phrase.

### Playback engine

The playback callback contains bounded DSP only: triangle bass, sine-triad pad, sine answering voice, synthetic drums, pitch-following drone, feedback shadow, onset pulse, and soft clipping. Current sounds are placeholders intended to validate interaction and timing.

## Threading rules

- SDL capture callback writes to lock-free single-reader rings.
- Main thread consumes analysis audio and updates atomics.
- Playback callback reads only atomics, immutable plans, and its own DSP state.
- Generated plans are never modified after publication.
- No allocation, file I/O, mutex, logging, or model inference is allowed in the playback callback.

## Future ML insertion points

Neural models are optional components, not the architecture itself.

### Phrase continuation model

Input: quantized note/onset sequence plus tonal estimate. Output: bass, chord, counter-melody, and event tokens. Candidate: small GRU/Transformer exported to ONNX or a custom integer runtime.

### Behaviour model

Input: recent RMS, pitch, onset density, silence duration, and user controls. Output: slow control trajectories for Ground, Shadow, Pulse, and Event roles.

### Tiny audio decoder

A reduced RAVE-like decoder may become one sound source. It must run in a worker with buffered output and must never be required for basic operation.

## Portability

SDL2 is the only runtime dependency in the alpha. Core analysis and composition code has no SDL dependency and is covered by native tests.
