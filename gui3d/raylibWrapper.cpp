#include "raylibWrapper.hpp"

// --- Vector3D Implementation ---
Vector3D::Vector3D(float x, float y, float z) : x(x), y(y), z(z) {}

Vector3 Vector3D::toRaylib() const { 
    return { x, y, z }; 
}

Vector2 Vector3D::toRaylib2D() const { 
    return { x, y }; 
}

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

Vector3D RaylibEngine::createVector(float x, float y, float z) {
    return Vector3D(x, y, z);
}

void RaylibEngine::moveVector(Vector3D& vec, float dx, float dy, float dz) {
    vec.x += dx;
    vec.y += dy;
    vec.z += dz;
}

Shape RaylibEngine::createShape(Vector3D pos, Vector3D size, Color color) {
    Shape s;
    s.type = Shape::SHAPE_CUBE;
    s.position = pos;
    s.size = size;
    s.color = color;
    return s;
}

Shape RaylibEngine::createTextureShape(Vector3D pos, const std::string& texturePath) {
    Shape s;
    s.type = Shape::SHAPE_TEXTURE2D;
    s.position = pos;
    s.texture = LoadTexture(texturePath.c_str());
    return s;
}

void RaylibEngine::changeTexture(Shape& shape, const std::string& newPath) {
    if (shape.type == Shape::SHAPE_TEXTURE2D) {
        UnloadTexture(shape.texture);
        shape.texture = LoadTexture(newPath.c_str());
    }
}

void RaylibEngine::moveShape(Shape& shape, float dx, float dy, float dz) {
    moveVector(shape.position, dx, dy, dz);
}

Shape RaylibEngine::load3DModel(Vector3D pos, const std::string& modelPath, const std::string& texturePath) {
    Shape s;
    s.type = Shape::SHAPE_MODEL3D;
    s.position = pos;
    s.model = LoadModel(modelPath.c_str());
    
    if (!texturePath.empty()) {
        s.texture = LoadTexture(texturePath.c_str());
        SetMaterialTexture(&s.model.materials[0], MATERIAL_MAP_DIFFUSE, s.texture);
    }
    return s;
}

void RaylibEngine::drawShape(const Shape& shape, Camera3D* camera) {
    if (shape.type == Shape::SHAPE_TEXTURE2D) {
        DrawTexture(shape.texture, shape.position.x, shape.position.y, WHITE);
    } 
    else if (camera != nullptr) {
        BeginMode3D(*camera);
        if (shape.type == Shape::SHAPE_CUBE) {
            DrawCube(shape.position.toRaylib(), shape.size.x, shape.size.y, shape.size.z, shape.color);
        } else if (shape.type == Shape::SHAPE_MODEL3D) {
            DrawModel(shape.model, shape.position.toRaylib(), 1.0f, WHITE);
        }
        EndMode3D();
    }
}

void RaylibEngine::unloadShape(Shape& shape) {
    if (shape.type == Shape::SHAPE_TEXTURE2D) {
        UnloadTexture(shape.texture);
    }
    if (shape.type == Shape::SHAPE_MODEL3D) {
        UnloadModel(shape.model);
        UnloadTexture(shape.texture);
    }
}