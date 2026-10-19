#ifdef __linux__
#include "platform/Platform.h"
#include <SDL2/SDL_keycode.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xfixes.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>

// Dois displays separados — sem race condition
// m_dpyUi  → thread principal + hotkey (XKeysymToKeycode / XQueryKeymap)
// m_dpyClk → thread do clicker (XSendEvent / XQueryPointer / XFixes)
class PlatformLinux : public Platform {
public:
    ~PlatformLinux() override {
        if (m_dpyUi)  XCloseDisplay(m_dpyUi);
        if (m_dpyClk) XCloseDisplay(m_dpyClk);
    }

    bool init() override {
        XInitThreads();
        m_dpyUi  = XOpenDisplay(nullptr);
        m_dpyClk = XOpenDisplay(nullptr);
        if (!m_dpyUi || !m_dpyClk) {
            fprintf(stderr, "XOpenDisplay failed: is DISPLAY set?\n");
            return false;
        }
        int ev, err, maj = 2, min = 0;
        m_xfixes = XFixesQueryExtension(m_dpyClk, &ev, &err) &&
                   XFixesQueryVersion(m_dpyClk, &maj, &min);
        return true;
    }

    bool updateTarget() override {
        auto now = std::chrono::steady_clock::now();
        if (now - m_winT < std::chrono::milliseconds(500)) return m_win != 0;
        m_winT = now;
        Window focused; int revert;
        XGetInputFocus(m_dpyClk, &focused, &revert);
        if (focused <= 1) { m_win = 0; return false; }
        char* name = nullptr;
        if (XFetchName(m_dpyClk, focused, &name) && name) {
            m_win = (strstr(name, "Minecraft") || strstr(name, "minecraft") ||
                     strstr(name, "CheatBreaker")) ? focused : 0;
            XFree(name);
        } else m_win = 0;
        return m_win != 0;
    }

    bool hasTarget() const override { return m_win != 0; }

    void sendClick() override {
        Window win = m_win;
        if (!win) return;
        Window root, child; int rx, ry, wx, wy; unsigned int mask;
        XQueryPointer(m_dpyClk, win, &root, &child, &rx, &ry, &wx, &wy, &mask);
        XEvent ev; memset(&ev, 0, sizeof(ev));
        ev.xbutton.button = 1; ev.xbutton.same_screen = True; ev.xbutton.subwindow = win;
        while (ev.xbutton.subwindow) {
            ev.xbutton.window = ev.xbutton.subwindow;
            XQueryPointer(m_dpyClk, ev.xbutton.window, &root, &ev.xbutton.subwindow,
                          &ev.xbutton.x_root, &ev.xbutton.y_root,
                          &ev.xbutton.x, &ev.xbutton.y, &ev.xbutton.state);
        }
        ev.type = ButtonPress;
        XSendEvent(m_dpyClk, ev.xbutton.window, True, ButtonPressMask, &ev); XFlush(m_dpyClk);
        ev.type = ButtonRelease;
        XSendEvent(m_dpyClk, ev.xbutton.window, True, ButtonReleaseMask, &ev); XFlush(m_dpyClk);
    }

    bool isLMBHeld() override {
        Window r, ch; int rx, ry, wx, wy; unsigned int mask;
        XQueryPointer(m_dpyClk, DefaultRootWindow(m_dpyClk), &r, &ch, &rx, &ry, &wx, &wy, &mask);
        return mask & Button1Mask;
    }

    // Em jogo o Minecraft troca o cursor por uma imagem transparente;
    // com GUI aberta volta o cursor normal. Cache de 50ms (XFixes aloca a imagem).
    bool isCursorVisible() override {
        if (!m_xfixes) return false;
        auto now = std::chrono::steady_clock::now();
        if (now - m_curT < std::chrono::milliseconds(50)) return m_curVisible;
        m_curT = now;
        XFixesCursorImage* img = XFixesGetCursorImage(m_dpyClk);
        if (!img) return m_curVisible = false;
        bool visible = false;
        for (int i = 0, n = img->width * img->height; i < n && !visible; i++)
            visible = (img->pixels[i] >> 24) & 0xFF;
        XFree(img);
        return m_curVisible = visible;
    }

    int mapKey(int k) override {
        KeySym ks = sdlToKeySym(k);
        return ks != NoSymbol ? XKeysymToKeycode(m_dpyUi, ks) : 0;
    }

    // XQueryKeymap — sem grab, sem interceptar a tecla
    bool isKeyDown(int kc) override {
        if (!kc) return false;
        char keys[32]{};
        XQueryKeymap(m_dpyUi, keys);
        return (keys[kc / 8] >> (kc % 8)) & 1;
    }

    const char* name() const override { return "Linux"; }

private:
    static KeySym sdlToKeySym(int k) {
        if (k >= 'a' && k <= 'z') return (KeySym)k;
        if (k >= '0' && k <= '9') return (KeySym)k;
        if (k >= SDLK_F1 && k <= SDLK_F12) return XK_F1 + (k - SDLK_F1);
        switch (k) {
            case SDLK_INSERT:   return XK_Insert;
            case SDLK_DELETE:   return XK_Delete;
            case SDLK_HOME:     return XK_Home;
            case SDLK_END:      return XK_End;
            case SDLK_PAGEUP:   return XK_Prior;
            case SDLK_PAGEDOWN: return XK_Next;
            case SDLK_RETURN:   return XK_Return;
            case SDLK_SPACE:    return XK_space;
            case SDLK_TAB:      return XK_Tab;
            default:            return NoSymbol;
        }
    }

    Display*                              m_dpyUi  = nullptr;
    Display*                              m_dpyClk = nullptr;
    bool                                  m_xfixes = false;
    std::atomic<Window>                   m_win{0};
    std::chrono::steady_clock::time_point m_winT{};
    std::chrono::steady_clock::time_point m_curT{};
    bool                                  m_curVisible = false;
};

std::unique_ptr<Platform> CreatePlatform() {
    return std::make_unique<PlatformLinux>();
}
#endif
