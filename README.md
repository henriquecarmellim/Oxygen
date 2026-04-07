# OxygenClicker

> Lightweight Linux AutoClicker with a clean ImGui interface.

![Platform](https://img.shields.io/badge/platform-Linux-blueviolet?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-blueviolet?style=flat-square)
![C++](https://img.shields.io/badge/C%2B%2B-17-blueviolet?style=flat-square)
![ImGui](https://img.shields.io/badge/ImGui-v1.91.6-blueviolet?style=flat-square)

---

## Features

- **Hold-to-click** — only clicks while LMB is held (toggle off for freeclick)
- **Randomized CPS** — set a min/max range, interval is randomized each click
- **Clean ImGui UI** — dark purple theme, no bloat
- **AppImage** — single portable binary, no install needed

---

## Download

Grab the latest `OxygenClicker-x86_64.AppImage` from [Releases](../../releases).

```bash
chmod +x OxygenClicker-x86_64.AppImage
./OxygenClicker-x86_64.AppImage
```

> **Note:** requires `libXtst` on the host (`sudo apt install libxtst6` on Debian/Ubuntu).

---

## Build from source

**Dependencies**

```bash
# Debian / Ubuntu
sudo apt install cmake build-essential libsdl2-dev libgl-dev libx11-dev libxtst-dev

# Arch
sudo pacman -S cmake sdl2 mesa libx11 libxtst
```

**Compile**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/OxygenClicker
```

**Build AppImage**

```bash
./build.sh
# outputs: OxygenClicker-x86_64.AppImage
```

---

## Stack

| Component | Detail |
|-----------|--------|
| Language  | C++17  |
| GUI       | Dear ImGui v1.91.6 (SDL2 + OpenGL3 backend) |
| Clicking  | XTest (`XTestFakeButtonEvent`) |
| Build     | CMake 3.16+, FetchContent for ImGui |
| Package   | AppImageKit |

---

## How it works

The clicker runs on a dedicated thread. It polls `XQueryPointer` to check if LMB is held (when *Hold to click* is enabled), then fires `XTestFakeButtonEvent` press+release via the XTest extension. The interval between clicks is randomized each time using `std::uniform_real_distribution` over the configured CPS range.

---

## License

MIT — see [LICENSE](LICENSE).
