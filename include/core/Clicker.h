#pragma once
#include "ClickerState.h"
#include "platform/Platform.h"
#include <thread>

class Clicker {
public:
    explicit Clicker(Platform* platform, ClickerState* state);
    ~Clicker();
    void start();
    void stop();
    bool isFocused() const;

private:
    void loop();
    Platform*     m_platform;
    ClickerState* m_state;
    std::thread   m_thread;
};
