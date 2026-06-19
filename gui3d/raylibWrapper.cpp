#include "raylibWrapper.hpp"

// --- RaylibEngine Implementation ---
RaylibEngine::RaylibEngine(int width, int height, const std::string& title) {
    InitWindow(width, height, title.c_str());
    SetTargetFPS(60);
}

RaylibEngine::~RaylibEngine() {
    CloseWindow();
}

bool RaylibEngine::shouldClose() const {
    return WindowShouldClose();
}

void RaylibEngine::beginDrawing() {
    BeginDrawing();
    ClearBackground(RAYWHITE);
}

void RaylibEngine::endDrawing() {
    EndDrawing();
}
