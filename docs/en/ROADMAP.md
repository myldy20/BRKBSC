# Roadmap

## 0.1 — prove the interaction

- microphone capture on macOS;
- monophonic pitch/onset analysis;
- record a phrase and generate a simple accompaniment;
- live Ground, Shadow, and Pulse behaviours;
- core tests and bilingual documentation.

Exit condition: humming a phrase produces a stable loop, and live mode reacts without audio dropouts.

## 0.2 — make it musically useful

- better note segmentation and octave correction;
- tap tempo and optional tempo inference;
- phrase-length detection and quantisation choices;
- several accompaniment personalities rather than random presets;
- proper envelopes, filters, saturation, and spatial processing;
- Memory, Density, Loyalty, and Tension controls;
- scene save/load and WAV export on desktop.

## 0.3 — Brick hardware build

- verify built-in microphone access and controls;
- add input gain calibration and feedback protection;
- aarch64 release build and PortMaster/NextUI packages;
- CPU, memory, latency, thermal, and battery profiling;
- stage-safe recovery after audio-device failure.

Exit condition: a 30-minute uninterrupted device session with acceptable latency and no xruns.

## 0.4 — musical memory

- capture and classify several phrase fragments;
- recall older material during silence;
- explicit Ground/Shadow/Pulse/Event mixer;
- freeze and mutate individual roles;
- transition logic for intros, interludes, and outros.

## 0.5 — tiny learned behaviour

- collect paired input/control/output sessions;
- train a small event or control-trajectory model;
- compare it blindly against deterministic rules;
- keep it only if musicians prefer it and Brick performance remains safe.

## Not planned early

No text-to-song generation, cloud dependency, on-device LLM, early polyphonic transcription, or procedural randomisation marketed as AI.
