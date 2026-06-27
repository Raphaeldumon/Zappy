#include "raylibWrapper.hpp"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cstdarg>
#include <vector>

// ---------------------------------------------------------------------------
// This translation unit is the sole point of contact with raylib. Everything
// above the facade speaks only gfx:: types; here we lower those to raylib
// calls and own every GPU/audio resource behind opaque integer handles.
// ---------------------------------------------------------------------------

namespace {

// gfx value types <-> raylib value types.
inline Vector2  toRl(gfx::Vec2 v)  { return { v.x, v.y }; }
inline Vector3  toRl(gfx::Vec3 v)  { return { v.x, v.y, v.z }; }
inline Color    toRl(gfx::Color c) { return { c.r, c.g, c.b, c.a }; }
inline gfx::Vec2 fromRl(Vector2 v) { return { v.x, v.y }; }
inline gfx::Vec3 fromRl(Vector3 v) { return { v.x, v.y, v.z }; }

Camera3D toRlCamera(const gfx::Camera& c)
{
    Camera3D cam{};
    cam.position   = toRl(c.position);
    cam.target     = toRl(c.target);
    cam.up         = toRl(c.up);
    cam.fovy       = c.fovy;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

int toRlKey(gfx::Key k)
{
    switch (k) {
        case gfx::Key::W:           return KEY_W;
        case gfx::Key::A:           return KEY_A;
        case gfx::Key::S:           return KEY_S;
        case gfx::Key::D:           return KEY_D;
        case gfx::Key::Up:          return KEY_UP;
        case gfx::Key::Down:        return KEY_DOWN;
        case gfx::Key::Left:        return KEY_LEFT;
        case gfx::Key::Right:       return KEY_RIGHT;
        case gfx::Key::Space:       return KEY_SPACE;
        case gfx::Key::LeftShift:   return KEY_LEFT_SHIFT;
        case gfx::Key::RightShift:  return KEY_RIGHT_SHIFT;
        case gfx::Key::LeftControl: return KEY_LEFT_CONTROL;
        case gfx::Key::C:           return KEY_C;
        case gfx::Key::R:           return KEY_R;
        case gfx::Key::F:           return KEY_F;
        case gfx::Key::P:           return KEY_P;
        case gfx::Key::M:           return KEY_M;
        case gfx::Key::H:           return KEY_H;
        case gfx::Key::Equal:       return KEY_EQUAL;
        case gfx::Key::Minus:       return KEY_MINUS;
        case gfx::Key::KpAdd:       return KEY_KP_ADD;
        case gfx::Key::KpSubtract:  return KEY_KP_SUBTRACT;
        case gfx::Key::PageUp:      return KEY_PAGE_UP;
        case gfx::Key::PageDown:    return KEY_PAGE_DOWN;
        case gfx::Key::End:         return KEY_END;
        case gfx::Key::Tab:         return KEY_TAB;
        case gfx::Key::F1:          return KEY_F1;
    }
    return KEY_NULL;
}

} // namespace

// ---------------------------------------------------------------------------
// gfx logging
// ---------------------------------------------------------------------------
namespace gfx {

void logWarn(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    TraceLog(LOG_WARNING, "%s", buf);
}

void logInfo(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    TraceLog(LOG_INFO, "%s", buf);
}

} // namespace gfx

// ---------------------------------------------------------------------------
// Impl — owns every raylib resource (kept out of the header behind PIMPL).
// Handles are indices; entries are never moved/removed mid-run, so a handle
// stays valid for the engine's lifetime. unload* tears the raylib resource
// down but leaves the slot (its handle simply stops being drawn).
// ---------------------------------------------------------------------------
struct RaylibEngine::Impl {
    struct AnimSet { ModelAnimation* anims{nullptr}; int count{0}; };

    std::vector<Model>     models;
    std::vector<Texture2D> textures;
    std::vector<Music>     musics;
    std::vector<AnimSet>   animSets;

    // Skybox (a unit cube + equirectangular shader, drawn camera-centred).
    Model     skybox{};
    Texture2D skyboxTex{};
    Shader    skyShader{};
    bool      skyboxLoaded{false};

    bool valid(int h, std::size_t n) const { return h >= 0 && static_cast<std::size_t>(h) < n; }
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
RaylibEngine::RaylibEngine(int width, int height, const std::string& title)
    : _impl(std::make_unique<Impl>())
{
    InitWindow(width, height, title.c_str());
    SetTargetFPS(60);
}

RaylibEngine::~RaylibEngine()
{
    // Free any GPU resources still loaded while the GL context is alive.
    unloadSkybox();
    for (auto& s : _impl->animSets)
        if (s.anims) UnloadModelAnimations(s.anims, s.count);
    for (auto& m : _impl->models)   if (m.meshCount > 0)        UnloadModel(m);
    for (auto& t : _impl->textures) if (t.id != 0)              UnloadTexture(t);
    for (auto& mu : _impl->musics)  if (mu.stream.buffer)       UnloadMusicStream(mu);
    CloseWindow();
}

bool RaylibEngine::shouldClose() const { return WindowShouldClose(); }

void RaylibEngine::beginDrawing()
{
    BeginDrawing();
    // The 360 background is a skybox drawn inside the 3D scene, so just clear.
    ClearBackground(BLACK);
}

void RaylibEngine::endDrawing()    { EndDrawing(); }
void RaylibEngine::disableCursor() { DisableCursor(); }
void RaylibEngine::enableCursor()  { EnableCursor(); }
int  RaylibEngine::screenWidth()  const { return GetScreenWidth(); }
int  RaylibEngine::screenHeight() const { return GetScreenHeight(); }
void RaylibEngine::drawFps(int x, int y) { DrawFPS(x, y); }

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
bool   RaylibEngine::keyDown(gfx::Key k)        const { return IsKeyDown(toRlKey(k)); }
bool   RaylibEngine::keyPressed(gfx::Key k)      const { return IsKeyPressed(toRlKey(k)); }
int    RaylibEngine::charPressed()               const { return GetCharPressed(); }
bool   RaylibEngine::mousePressed(gfx::MouseBtn) const { return IsMouseButtonPressed(MOUSE_BUTTON_LEFT); }
gfx::Vec2 RaylibEngine::mouseDelta()             const { return fromRl(GetMouseDelta()); }
float  RaylibEngine::mouseWheel()                const { return GetMouseWheelMove(); }
double RaylibEngine::time()                      const { return GetTime(); }
float  RaylibEngine::frameTime()                 const { return GetFrameTime(); }

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------
bool RaylibEngine::initAudio()
{
    InitAudioDevice();
    return IsAudioDeviceReady();
}

void RaylibEngine::closeAudio() { CloseAudioDevice(); }

gfx::MusicHandle RaylibEngine::loadMusic(const std::string& path)
{
    Music m = LoadMusicStream(path.c_str());
    if (m.stream.buffer == nullptr)
        return gfx::NoHandle;
    m.looping = true;
    _impl->musics.push_back(m);
    return static_cast<gfx::MusicHandle>(_impl->musics.size() - 1);
}

void RaylibEngine::playMusic(gfx::MusicHandle h)
{ if (_impl->valid(h, _impl->musics.size())) PlayMusicStream(_impl->musics[h]); }
void RaylibEngine::stopMusic(gfx::MusicHandle h)
{ if (_impl->valid(h, _impl->musics.size())) StopMusicStream(_impl->musics[h]); }
void RaylibEngine::pauseMusic(gfx::MusicHandle h)
{ if (_impl->valid(h, _impl->musics.size())) PauseMusicStream(_impl->musics[h]); }
void RaylibEngine::resumeMusic(gfx::MusicHandle h)
{ if (_impl->valid(h, _impl->musics.size())) ResumeMusicStream(_impl->musics[h]); }
void RaylibEngine::updateMusic(gfx::MusicHandle h)
{ if (_impl->valid(h, _impl->musics.size())) UpdateMusicStream(_impl->musics[h]); }
void RaylibEngine::setMusicVolume(gfx::MusicHandle h, float v)
{ if (_impl->valid(h, _impl->musics.size())) SetMusicVolume(_impl->musics[h], v); }

void RaylibEngine::unloadMusic(gfx::MusicHandle h)
{
    if (_impl->valid(h, _impl->musics.size()) && _impl->musics[h].stream.buffer) {
        UnloadMusicStream(_impl->musics[h]);
        _impl->musics[h] = {};
    }
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------
gfx::TextureHandle RaylibEngine::loadTexture(const std::string& path)
{
    Texture2D t = LoadTexture(path.c_str());
    if (t.id == 0)
        return gfx::NoHandle;
    _impl->textures.push_back(t);
    return static_cast<gfx::TextureHandle>(_impl->textures.size() - 1);
}

void RaylibEngine::setTextureBilinear(gfx::TextureHandle h)
{
    if (_impl->valid(h, _impl->textures.size()))
        SetTextureFilter(_impl->textures[h], TEXTURE_FILTER_BILINEAR);
}

void RaylibEngine::unloadTexture(gfx::TextureHandle h)
{
    if (_impl->valid(h, _impl->textures.size()) && _impl->textures[h].id != 0) {
        UnloadTexture(_impl->textures[h]);
        _impl->textures[h] = {};
    }
}

// ---------------------------------------------------------------------------
// Models
// ---------------------------------------------------------------------------
gfx::ModelHandle RaylibEngine::loadModel(const std::string& path)
{
    Model m = LoadModel(path.c_str());
    if (m.meshCount <= 0) {
        if (m.meshCount == 0) UnloadModel(m); // free any partial allocation
        return gfx::NoHandle;
    }
    _impl->models.push_back(m);
    return static_cast<gfx::ModelHandle>(_impl->models.size() - 1);
}

gfx::BBox RaylibEngine::modelBounds(gfx::ModelHandle h) const
{
    if (!_impl->valid(h, _impl->models.size()))
        return {};
    BoundingBox b = GetModelBoundingBox(_impl->models[h]);
    return { fromRl(b.min), fromRl(b.max) };
}

void RaylibEngine::translateModel(gfx::ModelHandle h, gfx::Vec3 t)
{
    if (!_impl->valid(h, _impl->models.size()))
        return;
    _impl->models[h].transform =
        MatrixMultiply(_impl->models[h].transform, MatrixTranslate(t.x, t.y, t.z));
}

void RaylibEngine::bindModelTexture(gfx::ModelHandle h, gfx::TextureHandle tex)
{
    if (!_impl->valid(h, _impl->models.size()) || !_impl->valid(tex, _impl->textures.size()))
        return;
    Model& m = _impl->models[h];
    // raylib prepends a default material at index 0 and puts the real glTF
    // materials at 1+, so apply to all of them (binding only [0] misses the mesh's).
    for (int i = 0; i < m.materialCount; ++i)
        SetMaterialTexture(&m.materials[i], MATERIAL_MAP_DIFFUSE, _impl->textures[tex]);
}

void RaylibEngine::unloadModel(gfx::ModelHandle h)
{
    if (_impl->valid(h, _impl->models.size()) && _impl->models[h].meshCount > 0) {
        UnloadModel(_impl->models[h]);
        _impl->models[h] = {};
    }
}

// ---------------------------------------------------------------------------
// Model animations
// ---------------------------------------------------------------------------
gfx::AnimSetHandle RaylibEngine::loadAnimations(const std::string& path)
{
    int count = 0;
    ModelAnimation* a = LoadModelAnimations(path.c_str(), &count);
    if (!a || count <= 0)
        return gfx::NoHandle;
    _impl->animSets.push_back({ a, count });
    return static_cast<gfx::AnimSetHandle>(_impl->animSets.size() - 1);
}

int RaylibEngine::animCount(gfx::AnimSetHandle s) const
{
    return _impl->valid(s, _impl->animSets.size()) ? _impl->animSets[s].count : 0;
}

std::string RaylibEngine::animName(gfx::AnimSetHandle s, int i) const
{
    if (!_impl->valid(s, _impl->animSets.size()) || i < 0 || i >= _impl->animSets[s].count)
        return {};
    return std::string(_impl->animSets[s].anims[i].name);
}

int RaylibEngine::animFrameCount(gfx::AnimSetHandle s, int i) const
{
    if (!_impl->valid(s, _impl->animSets.size()) || i < 0 || i >= _impl->animSets[s].count)
        return 0;
    return _impl->animSets[s].anims[i].keyframeCount;
}

void RaylibEngine::applyPose(gfx::ModelHandle m, gfx::AnimSetHandle s, int i, float frame)
{
    if (!_impl->valid(m, _impl->models.size()) || !_impl->valid(s, _impl->animSets.size()))
        return;
    if (i < 0 || i >= _impl->animSets[s].count)
        return;
    UpdateModelAnimation(_impl->models[m], _impl->animSets[s].anims[i], frame);
}

void RaylibEngine::unloadAnimations(gfx::AnimSetHandle s)
{
    if (_impl->valid(s, _impl->animSets.size()) && _impl->animSets[s].anims) {
        UnloadModelAnimations(_impl->animSets[s].anims, _impl->animSets[s].count);
        _impl->animSets[s] = {};
    }
}

// ---------------------------------------------------------------------------
// 3D drawing
// ---------------------------------------------------------------------------
void RaylibEngine::beginMode3D(const gfx::Camera& cam) { BeginMode3D(toRlCamera(cam)); }
void RaylibEngine::endMode3D() { EndMode3D(); }

void RaylibEngine::drawModelEx(gfx::ModelHandle h, gfx::Vec3 pos, gfx::Vec3 axis,
                               float angleDeg, gfx::Vec3 scale, gfx::Color tint)
{
    if (_impl->valid(h, _impl->models.size()) && _impl->models[h].meshCount > 0)
        DrawModelEx(_impl->models[h], toRl(pos), toRl(axis), angleDeg, toRl(scale), toRl(tint));
}

void RaylibEngine::drawCube(gfx::Vec3 pos, float w, float h, float l, gfx::Color c)
{ DrawCube(toRl(pos), w, h, l, toRl(c)); }
void RaylibEngine::drawPlane(gfx::Vec3 center, gfx::Vec2 size, gfx::Color c)
{ DrawPlane(toRl(center), toRl(size), toRl(c)); }
void RaylibEngine::drawSphere(gfx::Vec3 center, float radius, gfx::Color c)
{ DrawSphere(toRl(center), radius, toRl(c)); }
void RaylibEngine::drawLine3D(gfx::Vec3 a, gfx::Vec3 b, gfx::Color c)
{ DrawLine3D(toRl(a), toRl(b), toRl(c)); }

gfx::Vec2 RaylibEngine::worldToScreen(const gfx::Camera& cam, gfx::Vec3 world) const
{ return fromRl(GetWorldToScreen(toRl(world), toRlCamera(cam))); }

gfx::Ray RaylibEngine::screenToWorldRay(const gfx::Camera& cam, gfx::Vec2 screen) const
{
    Ray r = GetScreenToWorldRay(toRl(screen), toRlCamera(cam));
    return { fromRl(r.position), fromRl(r.direction) };
}

// ---------------------------------------------------------------------------
// 2D drawing
// ---------------------------------------------------------------------------
void RaylibEngine::drawText(const std::string& text, int x, int y, int fontSize, gfx::Color c)
{ DrawText(text.c_str(), x, y, fontSize, toRl(c)); }
void RaylibEngine::drawRect(int x, int y, int w, int h, gfx::Color c)
{ DrawRectangle(x, y, w, h, toRl(c)); }
void RaylibEngine::drawRectLines(int x, int y, int w, int h, gfx::Color c)
{ DrawRectangleLines(x, y, w, h, toRl(c)); }
void RaylibEngine::drawLine(int x1, int y1, int x2, int y2, gfx::Color c)
{ DrawLine(x1, y1, x2, y2, toRl(c)); }
void RaylibEngine::drawCircle(int cx, int cy, float radius, gfx::Color c)
{ DrawCircle(cx, cy, radius, toRl(c)); }

// ---------------------------------------------------------------------------
// Skybox (owns the cube + shader + texture and the GL state toggles)
// ---------------------------------------------------------------------------
bool RaylibEngine::loadSkybox(const std::string& png, const std::string& vs, const std::string& fs)
{
    if (!FileExists(png.c_str())) {
        TraceLog(LOG_WARNING, "loadSkybox: missing '%s' — no background", png.c_str());
        return false;
    }
    if (!FileExists(vs.c_str()) || !FileExists(fs.c_str())) {
        TraceLog(LOG_WARNING, "loadSkybox: missing shader files — no background");
        return false;
    }
    _impl->skyboxTex = LoadTexture(png.c_str());
    if (_impl->skyboxTex.id == 0) {
        TraceLog(LOG_WARNING, "loadSkybox: failed to load '%s'", png.c_str());
        return false;
    }
    SetTextureFilter(_impl->skyboxTex, TEXTURE_FILTER_BILINEAR);

    _impl->skyShader = LoadShader(vs.c_str(), fs.c_str());
    if (_impl->skyShader.id == 0) {
        TraceLog(LOG_WARNING, "loadSkybox: shader failed to compile — no background");
        UnloadTexture(_impl->skyboxTex);
        _impl->skyboxTex = {};
        return false;
    }

    // A unit cube: the fragment shader turns each cube-local position into a
    // view ray and samples the panorama equirectangularly.
    _impl->skybox = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    _impl->skybox.materials[0].shader = _impl->skyShader;
    _impl->skybox.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = _impl->skyboxTex;
    _impl->skyboxLoaded = true;
    TraceLog(LOG_INFO, "loadSkybox: '%s' ready", png.c_str());
    return true;
}

void RaylibEngine::drawSkybox()
{
    if (!_impl->skyboxLoaded)
        return;
    // Depth test/write and backface culling off: the box is centred on the eye
    // (translation dropped in the shader) and fills the view by direction, so
    // the rest of the scene paints over it.
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    rlDisableDepthTest();
    DrawModel(_impl->skybox, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

void RaylibEngine::unloadSkybox()
{
    if (_impl->skyboxLoaded) {
        UnloadModel(_impl->skybox); // does not free the texture map we assigned
        UnloadShader(_impl->skyShader);
    }
    if (_impl->skyboxTex.id != 0)
        UnloadTexture(_impl->skyboxTex);
    _impl->skyboxTex    = {};
    _impl->skyboxLoaded = false;
}

// ---------------------------------------------------------------------------
// Batched checkerboard floor (one rlBegin(RL_QUADS) per colour, chunked)
// ---------------------------------------------------------------------------
namespace {
void drawFloorBatch(Texture2D tex, bool wantDark, int w, int h,
                    float tileSize, float quadSize, float surfaceY)
{
    if (tex.id == 0)
        return;

    const float half = quadSize * 0.5f;
    const float y    = surfaceY;
    const int   kQuadsPerBatch = 1500; // 4 verts each, well under the 8192 buffer

    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlNormal3f(0.0f, 1.0f, 0.0f);

    int emitted = 0;
    for (int ty = 0; ty < h; ++ty) {
        for (int tx = 0; tx < w; ++tx) {
            if (((tx + ty) % 2 == 0) != wantDark)
                continue;
            if (emitted >= kQuadsPerBatch) {
                rlEnd(); // flush, then resume (texture stays bound across this)
                rlBegin(RL_QUADS);
                rlColor4ub(255, 255, 255, 255);
                rlNormal3f(0.0f, 1.0f, 0.0f);
                emitted = 0;
            }
            float wx = tx * tileSize + tileSize / 2.0f;
            float wz = ty * tileSize + tileSize / 2.0f;
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(wx - half, y, wz - half);
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(wx - half, y, wz + half);
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(wx + half, y, wz + half);
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(wx + half, y, wz - half);
            ++emitted;
        }
    }

    rlEnd();
    rlSetTexture(0);
}
} // namespace

void RaylibEngine::drawCheckerFloor(gfx::TextureHandle dark, gfx::TextureHandle light,
                                    int w, int h, float tileSize, float quadSize, float surfaceY)
{
    if (_impl->valid(dark, _impl->textures.size()))
        drawFloorBatch(_impl->textures[dark], true, w, h, tileSize, quadSize, surfaceY);
    if (_impl->valid(light, _impl->textures.size()))
        drawFloorBatch(_impl->textures[light], false, w, h, tileSize, quadSize, surfaceY);
}
