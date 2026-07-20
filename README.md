# BRKBSC

**An adaptive musical companion for TrimUI Brick and desktop.**

BRKBSC listens to a voice, instrument, room sound, or any other microphone input and turns it into musical material. It is designed for two related jobs:

1. **Compose** — hum a phrase and receive a playable accompaniment: rhythm, bass, harmony, and a small answering motif.
2. **Live** — let the device listen during a performance and grow a restrained drone, pulse, and delayed "shadow" around the performer.

The project deliberately starts with transparent, lightweight music algorithms. Small neural models may later replace individual decisions where they create a measurable musical improvement. The target is not "Suno on a handheld". The target is an instrument that reacts quickly, remains controllable, and can run reliably on a TrimUI Brick.

[Русская документация](README_RU.md)

## Current status

`0.1.0-alpha.1` is the first vertical prototype. It already contains:

- SDL2 microphone capture and audio playback;
- low-cost monophonic pitch and onset analysis;
- phrase recording from a microphone;
- key/mode estimation from recorded notes;
- generated 16-step drums, bass, chord pad, and answering melody;
- a live mode with input monitoring, pitch-following drone, onset pulse, and feedback shadow;
- keyboard and SDL game-controller input;
- a dependency-free bitmap UI suitable for 1024×768;
- core tests that do not require SDL2.

The complete prototype has now been heard successfully on a MacBook. It starts silently, automatically mutes the old arrangement while recording a replacement phrase, and restarts a new plan from the beginning. TrimUI Brick hardware remains untested.

It is an experiment, not a finished performance instrument. Pitch detection is currently monophonic, tempo is set manually, and the generated accompaniment is intentionally simple. The next stage replaces the single generic answer with controllable, role-based composition proposals.

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

## Controls

| Control | Compose mode | Live mode |
|---|---|---|
| `A` / Space | Start or stop phrase recording | Enable or mute the ghost |
| `B` / Backspace | Generate another accompaniment | Erase delay memory |
| `X` / Tab | Switch mode | Switch mode |
| `Y` / F | Freeze detected pitch | Freeze/unfreeze drone pitch |
| Start / Enter | Enable or mute output | Enable or mute output |
| D-pad Up/Down | Change BPM | Change BPM |
| D-pad Left/Right | Change agency | Change agency |
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
