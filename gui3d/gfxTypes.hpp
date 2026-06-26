#pragma once

// ---------------------------------------------------------------------------
// gfxTypes — neutral graphics vocabulary types, shared between the renderer
// facade (RaylibEngine, the only place raylib is included) and its callers.
//
// Nothing here includes raylib: these are plain PODs and helpers that mirror
// the handful of raylib value types the GUI passes around (vectors, colours,
// camera, rays), plus opaque integer handles standing in for the GPU/audio
// resources the facade owns internally. This is what lets interface.{hpp,cpp}
// stay completely raylib-free.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <string>

namespace gfx {

// --- Math / colour value types ---------------------------------------------
struct Vec2 { float x{0.0f}, y{0.0f}; };
struct Vec3 { float x{0.0f}, y{0.0f}, z{0.0f}; };

struct Color { std::uint8_t r{0}, g{0}, b{0}, a{255}; };

struct BBox { Vec3 min; Vec3 max; };
struct Ray  { Vec3 position; Vec3 direction; };

// A free-fly camera, lowered into a raylib Camera3D inside the facade.
struct Camera {
    Vec3  position{};
    Vec3  target{};
    Vec3  up{0.0f, 1.0f, 0.0f};
    float fovy{60.0f};
};

// --- Named colours (raylib's palette values, kept identical) ---------------
inline constexpr Color WHITE     {255, 255, 255, 255};
inline constexpr Color BLACK     {  0,   0,   0, 255};
inline constexpr Color RED       {230,  41,  55, 255};
inline constexpr Color YELLOW    {253, 249,   0, 255};
inline constexpr Color GOLD      {255, 203,   0, 255};
inline constexpr Color BEIGE     {211, 176, 131, 255};
inline constexpr Color RAYWHITE  {245, 245, 245, 255};
inline constexpr Color GRAY      {130, 130, 130, 255};
inline constexpr Color SKYBLUE   {102, 191, 255, 255};
inline constexpr Color LIGHTGRAY {200, 200, 200, 255};
inline constexpr Color GREEN     {  0, 228,  48, 255};

// --- Opaque resource handles (registry indices owned by the facade) --------
// -1 means "none / failed to load"; every facade draw call no-ops on it, so
// the missing-asset fallbacks (cube/skip) keep working unchanged.
using ModelHandle   = int;
using TextureHandle = int;
using ShaderHandle  = int;
using MusicHandle   = int;
using AnimSetHandle = int;
inline constexpr int NoHandle = -1;

// --- Input vocabulary (mapped to raylib key codes inside the facade) -------
enum class Key {
    W, A, S, D,
    Up, Down, Left, Right,
    Space, LeftShift, RightShift, LeftControl,
    C, R, F, P, M, H,
    Equal, Minus, KpAdd, KpSubtract,
    PageUp, PageDown, End, Tab, F1,
};
enum class MouseBtn { Left };

// --- Small vector helpers (replace the raymath calls used at runtime) ------
inline Vec3 add(Vec3 a, Vec3 b)   { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 sub(Vec3 a, Vec3 b)   { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 scale(Vec3 a, float s){ return { a.x * s, a.y * s, a.z * s }; }
inline float dot(Vec3 a, Vec3 b)  { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(Vec3 a)       { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(Vec3 a)
{
    const float len = length(a);
    return len > 0.0f ? scale(a, 1.0f / len) : a;
}

// --- printf-style string builder (replaces raylib's TextFormat) ------------
inline std::string fmt(const char* spec, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, spec);
    std::vsnprintf(buf, sizeof(buf), spec, ap);
    va_end(ap);
    return std::string(buf);
}

} // namespace gfx
