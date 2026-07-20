# BRKBSC

**An adaptive musical companion for TrimUI Brick and desktop.**

BRKBSC listens to a voice, instrument, room sound, or any other microphone input and turns it into musical material. It is designed for two related jobs:

1. **Compose** — hum a phrase and explore several musical interpretations with editable harmony, bass, rhythm, and answer roles.
2. **Live** — let the device listen during a performance and grow a restrained drone, pulse, and delayed "shadow" around the performer.

The project deliberately starts with transparent, lightweight music algorithms. Small neural models may later replace individual decisions where they create a measurable musical improvement. The target is not "Suno on a handheld". The target is an instrument that reacts quickly, remains controllable, and can run reliably on a TrimUI Brick.

[Русская документация](README_RU.md)

## Current status

`0.2.0-alpha` is the first co-composer iteration. It contains:

- SDL2 microphone capture and audio playback;
- low-cost monophonic pitch and onset analysis;
- silent phrase capture: the old arrangement is muted while a replacement idea is recorded;
- a 32-step musical plan divided into four melody-aware harmonic sections;
- scored four-note chord voicings with voice-leading;
- four deliberately different proposals: **Anchor**, **Reframe**, **Counter**, and **Break**;
- generated Harmony, Bass, Rhythm, and Answer roles;
- role pinning, muting, and regeneration;
- an Intent Compass: **Faithful ↔ Contrary** and **Still ↔ Driving**;
- live input monitoring, pitch-following drone, onset pulse, and feedback shadow;
- keyboard and SDL game-controller input;
- a dependency-free bitmap UI suitable for 1024×768;
- core tests that verify deterministic and distinct proposals.

The complete prototype has been heard successfully on a MacBook. TrimUI Brick hardware remains untested. The synthesis timbres and Live mode are still placeholders; the current development focus is composition quality and interaction.

## Quick start on macOS

```bash
brew install cmake sdl2
git clone https://github.com/myldy20/BRKBSC.git
cd BRKBSC
git switch prototype/0.1.0-alpha.1
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
open build/BRKBSC.app
```

macOS should request microphone access on the first launch. If it was denied, enable it in **System Settings → Privacy & Security → Microphone**.

To run tests without building the SDL application:

```bash
cmake -S . -B build-core -DBRKBSC_BUILD_APP=OFF
cmake --build build-core -j
ctest --test-dir build-core --output-on-failure
```

## Compose workflow

1. Press `A` / Space and hum a phrase. BRKBSC records in silence.
2. Press `A` / Space again. **Anchor** starts playing.
3. Press `B` to audition Reframe, Counter, Break, then Anchor again.
4. Move the D-pad to change the Intent Compass. The current proposal is rebuilt immediately.
5. Select a role with the shoulder buttons or `Q` / `E`.
6. Pin a good role with `Y` / `P`, mute it with Select / `M`, or regenerate the unpinned material with `R`.
7. Press `1`–`4` on a keyboard to jump directly to a proposal.

Pinned roles survive proposal changes and regeneration. Capturing a new phrase clears pins and mutes.

## Controls

| Control | Compose mode | Live mode |
|---|---|---|
| `A` / Space | Start or stop silent phrase capture | Enable or mute the ghost |
| `B` / Backspace | Next proposal | Erase delay memory |
| `X` / Tab | Switch mode | Switch mode |
| `Y` / `P` | Pin selected role | Freeze/unfreeze drone pitch |
| L/R shoulder or `Q` / `E` | Select role | — |
| Select or `M` | Mute selected role | — |
| `R` | Regenerate current proposal, preserving pinned roles | — |
| `1`–`4` | Select Anchor / Reframe / Counter / Break | — |
| D-pad | Intent Compass | BPM / Agency |
| `[` / `]` | Change BPM | Change BPM |
| Start / Enter | Enable or mute output | Enable or mute output |
| Escape | Quit | Quit |

Controller mappings on TrimUI Brick still need to be verified on real hardware.

## Documentation

- [Product concept](docs/en/CONCEPT.md)
- [AI companion design](docs/en/AI_COMPANION.md)
- [Architecture](docs/en/ARCHITECTURE.md)
- [Build and test guide](docs/en/BUILD.md)
- [Roadmap](docs/en/ROADMAP.md)

Russian versions are under [`docs/ru`](docs/ru).

## License

GPL-3.0-or-later.
