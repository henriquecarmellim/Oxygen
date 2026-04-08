#!/usr/bin/env bash
# build.sh — compila OxygenClicker e empacota como AppImage
# Usa Docker (Ubuntu 20.04 / GLIBC 2.31) para garantir compatibilidade
# com distros mais antigas (Pop!_OS, Ubuntu 20.04+, Fedora 33+, etc.)
set -e

APPDIR="AppDir"

# Se não tiver Docker, compila direto (sem garantia de compatibilidade GLIBC)
if ! command -v docker &>/dev/null; then
    echo "[warn] Docker não encontrado, compilando localmente..."
    cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build cmake-build-release --target OxygenClicker -j"$(nproc)"
else
    echo "==> Building inside Docker (Ubuntu 20.04 / GLIBC 2.31)..."
    docker run --rm \
        -v "$(pwd)":/src \
        -w /src \
        ubuntu:20.04 \
        bash -c "
            export DEBIAN_FRONTEND=noninteractive
            apt-get update -qq
            apt-get install -y -qq \
                cmake g++ git \
                libsdl2-dev libgl-dev \
                libx11-dev libxtst-dev \
                pkg-config ca-certificates 2>/dev/null
            rm -rf cmake-build-release
            cmake -B cmake-build-release \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_INSTALL_PREFIX=/usr
            cmake --build cmake-build-release --target OxygenClicker -j\$(nproc)
        "
fi

echo "==> Staging AppDir..."
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install cmake-build-release

# AppRun
cat > "$APPDIR/AppRun" << 'EOF'
#!/bin/sh
exec "$(dirname "$(readlink -f "$0")")/usr/bin/OxygenClicker" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# .desktop mínimo (obrigatório pelo AppImage spec)
cat > "$APPDIR/OxygenClicker.desktop" << 'EOF'
[Desktop Entry]
Name=OxygenClicker
Exec=OxygenClicker
Icon=OxygenClicker
Type=Application
Categories=Utility;
EOF

# ícone placeholder (1x1 PNG válido em base64)
echo "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==" \
    | base64 -d > "$APPDIR/OxygenClicker.png"

echo "==> Downloading appimagetool..."
if [ ! -f appimagetool-x86_64.AppImage ]; then
    wget -q "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x appimagetool-x86_64.AppImage
fi

echo "==> Packaging AppImage..."
ARCH=x86_64 ./appimagetool-x86_64.AppImage "$APPDIR" OxygenClicker-x86_64.AppImage

echo ""
echo "Done! -> OxygenClicker-x86_64.AppImage"
