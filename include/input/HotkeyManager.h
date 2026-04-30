#pragma once
#include "core/ClickerState.h"
#include <atomic>
#include <functional>
#include <thread>

class HotkeyManager {
public:
    using Callback = std::function<void()>;
    explicit HotkeyManager(ClickerState* state);
    ~HotkeyManager();
    void setToggleKey(int keycode);
    void setToggleCallback(Callback cb);
    void start();
    void stop();

private:
    void loop();
    ClickerState*     m_state;
    Callback          m_callback;
    int               m_keycode{0};
    std::thread       m_thread;
    std::atomic<bool> m_running{false};
};
