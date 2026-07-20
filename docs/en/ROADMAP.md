# Roadmap

## 0.1 — prove the interaction

- microphone capture on macOS;
- monophonic pitch/onset analysis;
- record a phrase and generate a simple accompaniment;
- live Ground, Shadow, and Pulse behaviours;
- core tests and bilingual documentation;
- silence on launch and while recording a replacement phrase.

Exit condition: humming a phrase produces a stable loop, and live mode reacts without audio dropouts.

## 0.2 — turn generation into proposals

- improve note segmentation and octave correction;
- tap tempo and optional tempo inference;
- phrase-length detection and quantisation choices;
- generate four meaningfully different proposals: Anchor, Reframe, Counter, and Break;
- audition one proposal at a time;
- mute, pin, and regenerate Harmony, Bass, Rhythm, and Answer independently;
- restart every accepted proposal cleanly from a musical boundary;
- save scenes and export MIDI/WAV on desktop.

Exit condition: the same phrase can produce several recognisably related but compositionally different directions, and the user can keep the good parts without losing them.

## 0.3 — Intent Compass

- add the Faithful ↔ Contrary and Still ↔ Driving performance surface;
- add Tension, Density, Evolution, Range, and tempo constraints;
- make controls affect generation decisions rather than post-processing only;
- expose a clear confidence/ambiguity view for the captured phrase;
- add A/B comparison and blind preference logging.

Exit condition: opposite control positions produce musically obvious and repeatable differences without reducing the system to genre presets.

## 0.4 — learned harmonic planner

- build an off-device Python training and evaluation pipeline;
- prepare permissively licensed melody/harmony/bass data;
- train a small causal GRU or Transformer conditioned on melody and intent;
- generate several ranked harmonic plans;
- compare the model blindly against deterministic harmony rules;
- export an int8 model with a deterministic fallback.

Exit condition: musicians prefer the learned harmonic proposals often enough to justify their complexity.

## 0.5 — phrase-based live companion

- detect phrase and bar boundaries instead of following every pitch continuously;
- plan one or two bars ahead in a worker thread;
- add Ground, Pulse, Answer, and Shadow role arming;
- reduce density while the performer is active and answer during space;
- retain and recall earlier motifs;
- freeze and mutate individual roles;
- add transition logic for intros, interludes, and outros.

Exit condition: live mode creates call-and-response and structural development rather than sounding like pitch tracking plus delay.

## 0.6 — learned roles

- train expressive drum generation from human MIDI performances;
- train bass and response generators conditioned on the harmonic plan;
- keep every role independently replaceable and mutable;
- compare learned and deterministic roles in blind tests;
- preserve hard CPU and latency budgets.

## 0.7 — Brick hardware build

- verify built-in microphone access and controls;
- add input gain calibration and feedback protection;
- add a lightweight ARM inference runtime only after desktop validation;
- produce aarch64 release builds and PortMaster/NextUI packages;
- profile CPU, memory, latency, thermals, and battery;
- recover safely after audio-device or inference failure.

Exit condition: a 30-minute uninterrupted device session with acceptable latency, no xruns, and enough CPU headroom for the renderer and UI.

## Not planned early

No text-to-song generation, cloud dependency, on-device LLM, early polyphonic transcription, or procedural randomisation marketed as AI. Direct neural waveform accompaniment remains a long-term research path, not the first Brick implementation.
