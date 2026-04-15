<div align="center">

<br/>

<img src="https://img.shields.io/badge/OxygenClicker-v2.0-8560E6?style=for-the-badge&labelColor=0a0a0d"/>

<br/><br/>

**Cross-platform AutoClicker with a clean ImGui interface.**

[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-8560E6?style=flat-square&logo=linux&logoColor=white)](https://github.com/henriquecarmellim/Oxygen)
[![License](https://img.shields.io/badge/license-Custom-8560E6?style=flat-square)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17-8560E6?style=flat-square)](src/main.cpp)
[![ImGui](https://img.shields.io/badge/ImGui-v1.91.6-8560E6?style=flat-square)](https://github.com/ocornut/imgui)

</div>

---

## Features

- **Hold-to-click** — only clicks while LMB is held (toggle off for freeclick)
- **Randomized CPS** — min/max range, interval randomized each click
- **Global keybind** — toggle from any window (F3)
- **Minecraft focus** — only clicks when target window is focused
- **Clean ImGui UI** — dark purple theme, Lexend font, no bloat
- **Cross-platform** — Linux (X11/XTest) and Windows (SendInput)

---

## Download

Grab the latest binaries from [**Releases**](../../releases).

| Platform | File |
|---|---|
| Linux x86_64 | `OxygenClicker-linux-x86_64` |
| Windows x86_64 | `OxygenClicker-windows-x86_64.exe` |

**Linux:**
```bash
chmod +x OxygenClicker-linux-x86_64
./OxygenClicker-linux-x86_64
```

> Requires `libXtst` on the host:
> ```bash
> # Debian / Ubuntu
> sudo apt install libxtst6
> # Arch
> sudo pacman -S libxtst
> ```

**Windows:** just run `OxygenClicker-windows-x86_64.exe` — no install needed, fully static.

---

## Build from source

**Linux dependencies**
```bash
# Debian / Ubuntu
sudo apt install cmake build-essential libsdl2-dev libgl-dev libx11-dev libxtst-dev

# Arch
sudo pacman -S cmake sdl2 mesa libx11 libxtst
```

**Linux build**
```bash
cmake -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j$(nproc)
./build-linux/OxygenClicker
```

**Windows cross-compile (from Linux, requires mingw-w64)**
```bash
# Download SDL2 mingw dev package from https://github.com/libsdl-org/SDL/releases
SDL2_DIR=/path/to/SDL2-x.x.x/x86_64-w64-mingw32

cmake -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DSDL2_INCLUDE_DIRS="$SDL2_DIR/include;$SDL2_DIR/include/SDL2" \
  -DSDL2_LIBRARIES="$SDL2_DIR/lib/libSDL2main.a;$SDL2_DIR/lib/libSDL2.a;-lwinmm;-limm32;-lole32;-loleaut32;-lversion;-lsetupapi" \
  -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static -lpthread"

cmake --build build-windows -j$(nproc)
```

---

## Architecture (v2.0)

```
src/
├── core/          # OS-independent clicker logic + state
├── platform/      # OS abstraction (Linux: XTest, Windows: SendInput)
├── input/         # Hotkey polling (Linux: XQueryKeymap, Windows: GetAsyncKeyState)
└── ui/            # ImGui UI (SDL2 + OpenGL3)
include/           # Headers for all modules
```

---

## Stack

| | |
|---|---|
| Language | C++17 |
| GUI | Dear ImGui v1.91.6 (SDL2 + OpenGL3) |
| Font | Lexend (Bold + Regular) |
| Clicking (Linux) | `XTestFakeButtonEvent` |
| Clicking (Windows) | `SendInput` |
| Global bind (Linux) | `XQueryKeymap` |
| Global bind (Windows) | `GetAsyncKeyState` |
| Build | CMake 3.16+, FetchContent |

---

## License

Free to use and modify. **Commercial sale of this software is not permitted.**  
See [LICENSE](LICENSE) for full terms.
