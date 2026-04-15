#ifdef _WIN32
#include <windows.h>

bool PollHotkey(int keycode) {
    return (GetAsyncKeyState(keycode) & 0x8000) != 0;
}
#endif
