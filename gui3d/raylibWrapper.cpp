#include "raylibWrapper.hpp"

// --- RaylibEngine Implementation ---
RaylibEngine::RaylibEngine(int width, int height, const std::string& title) {
    InitWindow(width, height, title.c_str());
    _background = LoadTexture("assets/Background.png");
    SetTargetFPS(60);
}

RaylibEngine::~RaylibEngine() {
    UnloadTexture(_background);
    CloseWindow();
}

bool RaylibEngine::shouldClose() const {
    return WindowShouldClose();
}

void RaylibEngine::beginDrawing() {
    BeginDrawing();
    ClearBackground(BLACK);
    Rectangle source = {
        0.0f,
        0.0f,
        static_cast<float>(_background.width),
        static_cast<float>(_background.height)
    };

    Rectangle dest = {
        0.0f,
        0.0f,
        static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight())
    };
    DrawTexturePro(_background, source, dest, Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
}

void RaylibEngine::endDrawing() {
    EndDrawing();
}
