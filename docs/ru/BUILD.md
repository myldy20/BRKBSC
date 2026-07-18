# Сборка и тестирование

## macOS

```bash
brew install cmake sdl2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
open build/BRKBSC.app
```

В bundle добавлено описание использования микрофона. При первом запуске macOS должна запросить разрешение.

Приватная статическая копия SDL2:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBRKBSC_FETCH_SDL2=ON
cmake --build build -j
```

## Linux

```bash
sudo apt install build-essential cmake pkg-config libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/brkbsc
```

## Тесты только ядра

```bash
cmake -S . -B build-core -DBRKBSC_BUILD_APP=OFF -DBRKBSC_BUILD_TESTS=ON
cmake --build build-core -j
ctest --test-dir build-core --output-on-failure
```

## TrimUI Brick

Первая альфа ещё не проверена с ALSA-устройством встроенного микрофона и реальной раскладкой кнопок Brick. План:

1. определить устройства захвата и воспроизведения;
2. проверить поддержку захвата в SDL2;
3. кросс-компилировать executable под `aarch64`;
4. добавить launcher для PortMaster/NextUI;
5. измерить нагрузку и задержку;
6. уменьшать буфер только после стабильной работы.

Не стоит уменьшать текущий буфер в 512 сэмплов до стабильного базового запуска.

## Опции CMake

| Опция | По умолчанию | Назначение |
|---|---:|---|
| `BRKBSC_BUILD_APP` | ON | Собирать SDL2-приложение |
| `BRKBSC_BUILD_TESTS` | ON | Собирать тесты ядра |
| `BRKBSC_FETCH_SDL2` | OFF | Скачать и собрать приватный SDL2 |
