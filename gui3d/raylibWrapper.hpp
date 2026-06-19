#pragma once
#include "raylib.h"
#include <string>

// Thin RAII wrapper around the raylib window / render lifecycle.
// Owns the window: construction opens it, destruction closes it.
// Non-copyable and non-movable because it manages a unique GL context —
// duplicating it would call InitWindow / CloseWindow more than once.
class RaylibEngine {
public:
    RaylibEngine(int width, int height, const std::string& title);
    ~RaylibEngine();

    RaylibEngine(const RaylibEngine&)            = delete;
    RaylibEngine& operator=(const RaylibEngine&) = delete;
    RaylibEngine(RaylibEngine&&)                 = delete;
    RaylibEngine& operator=(RaylibEngine&&)      = delete;

    bool shouldClose() const;
    void beginDrawing();
    void endDrawing();
};
