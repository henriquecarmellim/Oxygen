#!/usr/bin/env bash
# build.sh — compila OxygenClicker e empacota como AppImage
set -e

APPDIR="AppDir"
BINARY="cmake-build-release/OxygenClicker"

echo "==> Building..."
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build cmake-build-release --target OxygenClicker -j"$(nproc)"

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
