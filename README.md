<div align="center">

<br/>

<img src="https://img.shields.io/badge/OxygenClicker-v1.0-8560E6?style=for-the-badge&labelColor=0a0a0d"/>

<br/><br/>

**Lightweight Linux AutoClicker with a clean ImGui interface.**

[![Platform](https://img.shields.io/badge/platform-Linux-8560E6?style=flat-square&logo=linux&logoColor=white)](https://github.com/henriquecarmellim/Oxygen)
[![License](https://img.shields.io/badge/license-Custom-8560E6?style=flat-square)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17-8560E6?style=flat-square)](src/main.cpp)
[![ImGui](https://img.shields.io/badge/ImGui-v1.91.6-8560E6?style=flat-square)](https://github.com/ocornut/imgui)

</div>

---

## Features

- **Hold-to-click** — only clicks while LMB is held (toggle off for freeclick)
- **Randomized CPS** — min/max range, interval randomized each click
- **Global keybind** — toggle from any window via `XGrabKey`
- **Minecraft focus** — only clicks when Minecraft window is focused
- **Clean ImGui UI** — dark purple theme, Lexend font, no bloat
- **AppImage** — single portable binary, no install needed

---

## Download

Grab the latest `OxygenClicker-x86_64.AppImage` from [**Releases**](../../releases).

```bash
chmod +x OxygenClicker-x86_64.AppImage
./OxygenClicker-x86_64.AppImage
```

> Requires `libXtst` on the host:
> ```bash
> # Debian / Ubuntu
> sudo apt install libxtst6
> # Arch
> sudo pacman -S libxtst
> ```

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
# → OxygenClicker-x86_64.AppImage
```

---

## Stack

| | |
|---|---|
| Language | C++17 |
| GUI | Dear ImGui v1.91.6 (SDL2 + OpenGL3) |
| Font | Lexend (Bold + Regular) |
| Clicking | `XSendEvent` ButtonPress/Release |
| Global bind | `XGrabKey` on root window |
| Focus check | `XGetInputFocus` + `XFetchName` |
| Build | CMake 3.16+, FetchContent |
| Package | AppImageKit |

---

## How it works

The clicker runs on a **dedicated thread** with its own `Display*` connection — completely separate from the UI thread to avoid any race conditions or freezes.

Click injection uses `XSendEvent` with `ButtonPress/Release` events sent directly to the focused Minecraft subwindow (same approach used in production JNI clients).

The global keybind uses `XGrabKey` on the root window, so the hotkey fires regardless of which window has focus.

---

## License

Free to use and modify. **Commercial sale of this software is not permitted.**  
See [LICENSE](LICENSE) for full terms.
