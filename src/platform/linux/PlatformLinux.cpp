#ifdef __linux__
#include "platform/Platform.h"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <cstring>

class PlatformLinux : public Platform {
public:
    PlatformLinux()  { m_display = XOpenDisplay(nullptr); }
    ~PlatformLinux() { if (m_display) XCloseDisplay(m_display); }

    void sendClick(int button) override {
        if (!m_display) return;
        XTestFakeButtonEvent(m_display, button, True,  CurrentTime);
        XTestFakeButtonEvent(m_display, button, False, CurrentTime);
        XFlush(m_display);
    }

    bool isTargetWindowFocused() override {
        if (!m_display || !m_targetName[0]) return true;
        Window focused; int revert;
        XGetInputFocus(m_display, &focused, &revert);
        char* name = nullptr;
        XFetchName(m_display, focused, &name);
        bool match = name && strstr(name, m_targetName);
        if (name) XFree(name);
        return match;
    }

    void setTargetWindowName(const char* name) override {
        strncpy(m_targetName, name, sizeof(m_targetName) - 1);
    }

private:
    Display* m_display{nullptr};
    char     m_targetName[256]{};
};

std::unique_ptr<Platform> CreatePlatform() {
    return std::make_unique<PlatformLinux>();
}
#endif
