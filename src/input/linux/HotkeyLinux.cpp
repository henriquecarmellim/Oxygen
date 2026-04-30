#ifdef __linux__
#include <X11/Xlib.h>

bool PollHotkey(int keycode) {
    static Display* dpy = XOpenDisplay(nullptr);
    if (!dpy || !keycode) return false;
    char keys[32]{};
    XQueryKeymap(dpy, keys);
    return (keys[keycode / 8] >> (keycode % 8)) & 1;
}
#endif
