#pragma once
#include "core/ClickerState.h"
#include "core/Clicker.h"
#include "input/HotkeyManager.h"

class UI {
public:
    UI(ClickerState* state, Clicker* clicker, HotkeyManager* hotkey);
    void run();

private:
    void render();
    ClickerState*  m_state;
    Clicker*       m_clicker;
    HotkeyManager* m_hotkey;
};
