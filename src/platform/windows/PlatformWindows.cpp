#ifdef _WIN32
#include "platform/Platform.h"
#include <SDL2/SDL_keycode.h>
#include <windows.h>
#include <timeapi.h>
#include <atomic>
#include <cstring>

class PlatformWindows : public Platform {
public:
    ~PlatformWindows() override { timeEndPeriod(1); }

    // Sem isso Sleep(1)/sleep_for(1ms) dorme ~15.6ms e o CPS fica travado
    bool init() override { timeBeginPeriod(1); return true; }

    // LWJGL2 (1.8 e clients tipo Lunar/Badlion) → classe "LWJGL"
    // LWJGL3 / GLFW (1.13+)                      → classe "GLFW30"
    bool updateTarget() override {
        HWND fg = GetForegroundWindow();
        char cls[64]{};
        if (fg) GetClassNameA(fg, cls, sizeof(cls));
        bool mc = !strcmp(cls, "LWJGL") || !strncmp(cls, "GLFW", 4);
        m_hwnd = mc ? fg : nullptr;
        return mc;
    }

    bool hasTarget() const override { return m_hwnd.load() != nullptr; }

    // PostMessage em vez de SendInput: não altera o estado físico do botão,
    // então GetAsyncKeyState(VK_LBUTTON) continua refletindo o mouse real (Hold to click).
    void sendClick() override {
        HWND hwnd = m_hwnd;
        if (!hwnd) return;
        POINT p; GetCursorPos(&p); ScreenToClient(hwnd, &p);
        LPARAM lp = MAKELPARAM(p.x, p.y);   // importa no inventário (posição do slot)
        PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
        PostMessageA(hwnd, WM_LBUTTONUP,   0,          lp);
    }

    bool isLMBHeld() override {
        // respeita "inverter botões" do Windows
        int vk = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
        return GetAsyncKeyState(vk) & 0x8000;
    }

    // Em jogo o Minecraft esconde o cursor e prende ele na janela (ClipCursor).
    // Com GUI aberta o cursor aparece e fica livre.
    bool isCursorVisible() override {
        CURSORINFO ci{};
        ci.cbSize = sizeof(ci);
        if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING) || !ci.hCursor)
            return false;
        RECT clip, screen;
        GetClipCursor(&clip);
        screen.left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
        screen.top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
        screen.right  = screen.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        screen.bottom = screen.top  + GetSystemMetrics(SM_CYVIRTUALSCREEN);
        bool clipped = clip.left > screen.left || clip.top > screen.top ||
                       clip.right < screen.right || clip.bottom < screen.bottom;
        return !clipped;
    }

    int mapKey(int k) override {
        if (k >= 'a' && k <= 'z') return 'A' + (k - 'a');
        if (k >= '0' && k <= '9') return k;
        if (k >= SDLK_F1 && k <= SDLK_F12) return VK_F1 + (k - SDLK_F1);
        switch (k) {
            case SDLK_INSERT:   return VK_INSERT;
            case SDLK_DELETE:   return VK_DELETE;
            case SDLK_HOME:     return VK_HOME;
            case SDLK_END:      return VK_END;
            case SDLK_PAGEUP:   return VK_PRIOR;
            case SDLK_PAGEDOWN: return VK_NEXT;
            case SDLK_RETURN:   return VK_RETURN;
            case SDLK_SPACE:    return VK_SPACE;
            case SDLK_TAB:      return VK_TAB;
            default:            return 0;
        }
    }

    // GetAsyncKeyState — polling, não intercepta a tecla
    bool isKeyDown(int vk) override {
        return vk && (GetAsyncKeyState(vk) & 0x8000);
    }

    const char* name() const override { return "Windows"; }

private:
    std::atomic<HWND> m_hwnd{nullptr};
};

std::unique_ptr<Platform> CreatePlatform() {
    return std::make_unique<PlatformWindows>();
}
#endif
