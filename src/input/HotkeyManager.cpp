#include "input/HotkeyManager.h"
#include <chrono>

HotkeyManager::HotkeyManager(ClickerState* state) : m_state(state) {}
HotkeyManager::~HotkeyManager() { stop(); }

void HotkeyManager::setToggleKey(int keycode)      { m_keycode = keycode; }
void HotkeyManager::setToggleCallback(Callback cb) { m_callback = std::move(cb); }

void HotkeyManager::start() {
    m_running = true;
    m_thread  = std::thread(&HotkeyManager::loop, this);
}

void HotkeyManager::stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

extern bool PollHotkey(int keycode);

void HotkeyManager::loop() {
    bool prev = false;
    while (m_running) {
        bool cur = PollHotkey(m_keycode);
        if (cur && !prev) {
            m_state->enabled = !m_state->enabled;
            if (m_callback) m_callback();
        }
        prev = cur;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
