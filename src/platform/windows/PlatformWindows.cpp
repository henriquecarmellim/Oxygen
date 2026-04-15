#ifdef _WIN32
#include "platform/Platform.h"
#include <windows.h>
#include <cstring>

class PlatformWindows : public Platform {
public:
    void sendClick(int button) override {
        INPUT inputs[2]{};
        inputs[0].type       = INPUT_MOUSE;
        inputs[0].mi.dwFlags = (button == 1) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
        inputs[1].type       = INPUT_MOUSE;
        inputs[1].mi.dwFlags = (button == 1) ? MOUSEEVENTF_LEFTUP   : MOUSEEVENTF_RIGHTUP;
        SendInput(2, inputs, sizeof(INPUT));
    }

    bool isTargetWindowFocused() override {
        if (!m_targetName[0]) return true;
        HWND hwnd = GetForegroundWindow();
        char title[256]{};
        GetWindowTextA(hwnd, title, sizeof(title));
        return strstr(title, m_targetName) != nullptr;
    }

    void setTargetWindowName(const char* name) override {
        strncpy(m_targetName, name, sizeof(m_targetName) - 1);
    }

private:
    char m_targetName[256]{};
};

std::unique_ptr<Platform> CreatePlatform() {
    return std::make_unique<PlatformWindows>();
}
#endif
