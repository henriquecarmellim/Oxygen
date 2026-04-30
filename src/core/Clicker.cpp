#include "core/Clicker.h"
#include <chrono>
#include <random>

Clicker::Clicker(Platform* platform, ClickerState* state)
    : m_platform(platform), m_state(state) {}

Clicker::~Clicker() { stop(); }

void Clicker::start() {
    m_state->running = true;
    m_thread = std::thread(&Clicker::loop, this);
}

void Clicker::stop() {
    m_state->running = false;
    if (m_thread.joinable()) m_thread.join();
}

bool Clicker::isFocused() const {
    return m_platform->isTargetWindowFocused();
}

void Clicker::loop() {
    std::mt19937 rng(std::random_device{}());
    while (m_state->running) {
        bool shouldClick = m_state->enabled &&
            (!m_state->holdMode || m_state->holdActive) &&
            (!m_state->focusOnly || m_platform->isTargetWindowFocused());

        if (shouldClick) {
            m_platform->sendClick(1);
            int lo = m_state->cpsMin.load();
            int hi = m_state->cpsMax.load();
            if (lo > hi) std::swap(lo, hi);
            int cps = std::uniform_int_distribution<int>(lo, hi)(rng);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / std::max(cps, 1)));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
