#ifdef _WIN32
#include "platform/Platform.h"
#include <SDL2/SDL_keycode.h>
#include <windows.h>
#include <timeapi.h>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <vector>

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
    // Em tela cheia (F11) com 1 monitor o clip == tela inteira, então só o clip
    // não basta: o LWJGL2 troca por um cursor transparente quando trava o mouse.
    bool isCursorVisible() override {
        CURSORINFO ci{};
        ci.cbSize = sizeof(ci);
        if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING) || !ci.hCursor)
            return false;
        if (ci.hCursor != m_lastCursor) {   // handle de cursor é estável, cacheia
            m_lastCursor = ci.hCursor;
            m_lastBlank  = isBlankCursor(ci.hCursor);
        }
        if (m_lastBlank) return false;
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
    static std::vector<uint32_t> readBitmap(HDC dc, HBITMAP bmp, int& w, int& h) {
        BITMAP bm{};
        GetObject(bmp, sizeof(bm), &bm);
        w = bm.bmWidth; h = bm.bmHeight;
        BITMAPINFO bi{};
        bi.bmiHeader.biSize        = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth       = w;
        bi.bmiHeader.biHeight      = -h;   // top-down
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        std::vector<uint32_t> px((size_t)w * h);
        if (!GetDIBits(dc, bmp, 0, h, px.data(), &bi, DIB_RGB_COLORS)) px.clear();
        return px;
    }

    // Cursor 100% transparente = mouse travado pelo jogo
    static bool isBlankCursor(HCURSOR cur) {
        ICONINFO ii{};
        if (!GetIconInfo(cur, &ii)) return false;
        HDC dc = GetDC(nullptr);
        int mw, mh, cw, ch;
        auto mask = readBitmap(dc, ii.hbmMask, mw, mh);
        bool blank = !mask.empty();
        if (ii.hbmColor) {
            auto color = readBitmap(dc, ii.hbmColor, cw, ch);
            bool hasAlpha = false;
            for (uint32_t p : color) if (p >> 24) { hasAlpha = true; break; }
            if (hasAlpha) {
                blank = false;                       // tem pixel com alpha > 0
            } else {
                for (uint32_t p : color) if (p & 0xFFFFFF) { blank = false; break; }
                for (uint32_t p : mask)  if ((p & 0xFFFFFF) != 0xFFFFFF) { blank = false; break; }
            }
            if (color.empty()) blank = false;
        } else {
            // monocromático: metade de cima = AND, metade de baixo = XOR
            size_t half = mask.size() / 2;
            for (size_t i = 0; i < mask.size() && blank; ++i) {
                uint32_t v = mask[i] & 0xFFFFFF;
                if (i < half ? v != 0xFFFFFF : v != 0) blank = false;
            }
        }
        ReleaseDC(nullptr, dc);
        if (ii.hbmMask)  DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        return blank;
    }

    std::atomic<HWND> m_hwnd{nullptr};
    HCURSOR m_lastCursor = nullptr;
    bool    m_lastBlank  = false;
};

std::unique_ptr<Platform> CreatePlatform() {
    return std::make_unique<PlatformWindows>();
}
#endif
