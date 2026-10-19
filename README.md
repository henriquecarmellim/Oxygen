<div align="center">

<br/>

<img src="https://img.shields.io/badge/OxygenClicker-v2.1-8560E6?style=for-the-badge&labelColor=0a0a0d"/>

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
- **Work in inventory** — optionally keep clicking while a GUI (inventory, chest) is open; off = pauses in GUIs
- **Clean ImGui UI** — dark purple theme, Lexend font, no bloat
- **Cross-platform** — Linux (X11) and Windows (Win32), clicks posted straight to the game window

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

> Requires `libXfixes` on the host (usually already installed):
> ```bash
> # Debian / Ubuntu
> sudo apt install libxfixes3
> # Arch
> sudo pacman -S libxfixes
> ```

**Windows:** just run `OxygenClicker-windows-x86_64.exe` — no install needed, fully static.

---

## Build from source

**Linux dependencies**
```bash
# Debian / Ubuntu
sudo apt install cmake build-essential libsdl2-dev libgl-dev libx11-dev libxfixes-dev

# Arch
sudo pacman -S cmake sdl2 mesa libx11 libxfixes
```

**Linux build**
```bash
cmake -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j$(nproc)
./build-linux/OxygenClicker
```

**Windows native build (MSYS2 MinGW64 shell)**
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-SDL2
cmake -G Ninja -B build-windows -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows
```

**Windows cross-compile (from Linux, requires mingw-w64)**
```bash
# Download SDL2 mingw dev package from https://github.com/libsdl-org/SDL/releases
SDL2_DIR=/path/to/SDL2-x.x.x/x86_64-w64-mingw32

PKG_CONFIG_LIBDIR=$SDL2_DIR/lib/pkgconfig cmake -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DPKG_CONFIG_ARGN=--define-prefix

cmake --build build-windows -j$(nproc)
```

The Windows `.exe` is fully static (SDL2 + MinGW runtime linked in).

---

## Architecture (v2.1)

```
src/
├── main.cpp                       # ImGui UI, clicker loop, hotkey loop (OS-independent)
└── platform/
    ├── linux/PlatformLinux.cpp    # X11: XSendEvent, XQueryKeymap, XFixes cursor
    └── windows/PlatformWindows.cpp# Win32: PostMessage, GetAsyncKeyState, GetCursorInfo
include/platform/Platform.h        # OS abstraction interface
```

---

## Stack

| | |
|---|---|
| Language | C++17 |
| GUI | Dear ImGui v1.91.6 (SDL2 + OpenGL3) |
| Font | Lexend (Bold + Regular) |
| Clicking (Linux) | `XSendEvent` to the game window |
| Clicking (Windows) | `PostMessage(WM_LBUTTONDOWN/UP)` to the game window |
| Global bind (Linux) | `XQueryKeymap` |
| Global bind (Windows) | `GetAsyncKeyState` |
| GUI detection (Linux) | `XFixesGetCursorImage` (transparent cursor = in game) |
| GUI detection (Windows) | `GetCursorInfo` + `GetClipCursor` (hidden/clipped = in game) |
| Build | CMake 3.16+, FetchContent |

---

## License

Free to use and modify. **Commercial sale of this software is not permitted.**  
See [LICENSE](LICENSE) for full terms.
