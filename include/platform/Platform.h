#pragma once
#include <memory>

class Platform {
public:
    virtual ~Platform() = default;
    virtual void sendClick(int button = 1) = 0;
    virtual bool isTargetWindowFocused() = 0;
    virtual void setTargetWindowName(const char* name) = 0;
};

std::unique_ptr<Platform> CreatePlatform();
