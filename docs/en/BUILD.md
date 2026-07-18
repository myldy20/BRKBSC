# Build and test guide

## macOS

```bash
brew install cmake sdl2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
open build/BRKBSC.app
```

The app bundle contains a microphone usage description. macOS should ask for permission on first launch.

A private static SDL2 build is also supported:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBRKBSC_FETCH_SDL2=ON
cmake --build build -j
```

## Linux desktop

```bash
sudo apt install build-essential cmake pkg-config libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/brkbsc
```

## Core-only tests

```bash
cmake -S . -B build-core -DBRKBSC_BUILD_APP=OFF -DBRKBSC_BUILD_TESTS=ON
cmake --build build-core -j
ctest --test-dir build-core --output-on-failure
```

## TrimUI Brick

The alpha has not yet been validated against the Brick microphone ALSA device and controller mapping. The intended process is:

1. identify capture and playback devices on the target firmware;
2. confirm SDL2 audio capture support;
3. cross-compile the same executable for `aarch64`;
4. add PortMaster/NextUI launchers and device environment variables;
5. profile analysis and callback load;
6. tune buffer size only after stable operation.

Do not lower the 512-sample buffer before the baseline is stable.

## CMake options

| Option | Default | Purpose |
|---|---:|---|
| `BRKBSC_BUILD_APP` | ON | Build SDL2 application |
| `BRKBSC_BUILD_TESTS` | ON | Build core tests |
| `BRKBSC_FETCH_SDL2` | OFF | Download and build private SDL2 |
