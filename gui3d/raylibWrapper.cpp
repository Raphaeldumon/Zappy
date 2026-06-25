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
    // The 360 background is now a skybox drawn inside the 3D scene (see
    // Interface::render), so just clear to black here.
    ClearBackground(BLACK);
}

void RaylibEngine::endDrawing() {
    EndDrawing();
}
