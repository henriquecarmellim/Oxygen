#pragma once
#include <memory>

// Tudo que depende do sistema (X11 / Win32) fica atrás desta interface.
// updateTarget/sendClick/isLMBHeld/isCursorVisible → thread do clicker
// mapKey → thread principal, isKeyDown → thread do hotkey
class Platform {
public:
    virtual ~Platform() = default;
    virtual bool init() = 0;
    virtual bool updateTarget() = 0;          // janela em foco é o Minecraft?
    virtual bool hasTarget() const = 0;       // último resultado de updateTarget (thread-safe)
    virtual void sendClick() = 0;             // clique esquerdo na janela alvo
    virtual bool isLMBHeld() = 0;             // estado físico do botão esquerdo
    virtual bool isCursorVisible() = 0;       // true = GUI/inventário aberto
    virtual int  mapKey(int sdlKey) = 0;      // SDL_Keycode → código nativo (0 = sem bind)
    virtual bool isKeyDown(int nativeKey) = 0;
    virtual const char* name() const = 0;
};

std::unique_ptr<Platform> CreatePlatform();
