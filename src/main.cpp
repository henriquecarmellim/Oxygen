#include "platform/Platform.h"
#include "core/ClickerState.h"
#include "core/Clicker.h"
#include "input/HotkeyManager.h"
#include "ui/UI.h"

int main() {
    auto platform = CreatePlatform();
    platform->setTargetWindowName("Minecraft");

    ClickerState  state;
    Clicker       clicker(platform.get(), &state);
    HotkeyManager hotkey(&state);

#ifdef _WIN32
    hotkey.setToggleKey(0x72); // VK_F3
#else
    hotkey.setToggleKey(69);   // F3 keycode on X11
#endif
    hotkey.start();
    clicker.start();

    UI ui(&state, &clicker, &hotkey);
    ui.run();

    clicker.stop();
    hotkey.stop();
    return 0;
}
