#pragma once
#include <atomic>

struct ClickerState {
    std::atomic<bool> running{false};
    std::atomic<bool> enabled{false};
    std::atomic<bool> holdMode{true};
    std::atomic<bool> holdActive{false};
    std::atomic<bool> focusOnly{true};
    std::atomic<int>  cpsMin{8};
    std::atomic<int>  cpsMax{12};
};
