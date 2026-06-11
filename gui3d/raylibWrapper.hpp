#pragma once
#include "raylib.h"
#include <string>

struct Vector3D {
    float x, y, z;
    Vector3D(float x = 0, float y = 0, float z = 0);
    
    Vector3 toRaylib() const;
    Vector2 toRaylib2D() const;
};

struct Shape {
    enum Type { SHAPE_CUBE, SHAPE_SPHERE, SHAPE_TEXTURE2D, SHAPE_MODEL3D };
    Type type;
    Vector3D position;
    Vector3D size; 
    Texture2D texture;
    Model model;
    Color color;
};

class RaylibEngine {
public:
    RaylibEngine(int width, int height, const std::string& title);
    ~RaylibEngine();

    bool shouldClose() const;
    void beginDrawing();
    void endDrawing();

    // Vector Functions
    Vector3D createVector(float x, float y, float z = 0.0f);
    void moveVector(Vector3D& vec, float dx, float dy, float dz = 0.0f);

    // Shape & Texture Functions
    Shape createShape(Vector3D pos, Vector3D size, Color color = BLUE);
    Shape createTextureShape(Vector3D pos, const std::string& texturePath);
    void changeTexture(Shape& shape, const std::string& newPath);
    void moveShape(Shape& shape, float dx, float dy, float dz = 0.0f);

    // 3D Model Functions
    Shape load3DModel(Vector3D pos, const std::string& modelPath, const std::string& texturePath = "");

    // Render & Memory Management
    void drawShape(const Shape& shape, Camera3D* camera = nullptr);
    void unloadShape(Shape& shape);
};