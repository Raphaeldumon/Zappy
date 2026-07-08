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

namespace
{

// gfx value types <-> raylib value types.
inline Vector2 toRl(gfx::Vec2 v)
{
    return {v.x, v.y};
}
inline Vector3 toRl(gfx::Vec3 v)
{
    return {v.x, v.y, v.z};
}
inline Color toRl(gfx::Color c)
{
    return {c.r, c.g, c.b, c.a};
}
inline gfx::Vec2 fromRl(Vector2 v)
{
    return {v.x, v.y};
}
inline gfx::Vec3 fromRl(Vector3 v)
{
    return {v.x, v.y, v.z};
}

Camera3D toRlCamera(const gfx::Camera &c)
{
    Camera3D cam{};
    cam.position = toRl(c.position);
    cam.target = toRl(c.target);
    cam.up = toRl(c.up);
    cam.fovy = c.fovy;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

int toRlKey(gfx::Key k)
{
    switch (k)
    {
    case gfx::Key::W:
        return KEY_W;
    case gfx::Key::A:
        return KEY_A;
    case gfx::Key::S:
        return KEY_S;
    case gfx::Key::D:
        return KEY_D;
    case gfx::Key::Up:
        return KEY_UP;
    case gfx::Key::Down:
        return KEY_DOWN;
    case gfx::Key::Left:
        return KEY_LEFT;
    case gfx::Key::Right:
        return KEY_RIGHT;
    case gfx::Key::Space:
        return KEY_SPACE;
    case gfx::Key::Enter:
        return KEY_ENTER;
    case gfx::Key::Escape:
        return KEY_ESCAPE;
    case gfx::Key::LeftShift:
        return KEY_LEFT_SHIFT;
    case gfx::Key::RightShift:
        return KEY_RIGHT_SHIFT;
    case gfx::Key::LeftControl:
        return KEY_LEFT_CONTROL;
    case gfx::Key::C:
        return KEY_C;
    case gfx::Key::R:
        return KEY_R;
    case gfx::Key::F:
        return KEY_F;
    case gfx::Key::P:
        return KEY_P;
    case gfx::Key::M:
        return KEY_M;
    case gfx::Key::T:
        return KEY_T;
    case gfx::Key::H:
        return KEY_H;
    case gfx::Key::V:
        return KEY_V;
    case gfx::Key::Equal:
        return KEY_EQUAL;
    case gfx::Key::Minus:
        return KEY_MINUS;
    case gfx::Key::KpAdd:
        return KEY_KP_ADD;
    case gfx::Key::KpSubtract:
        return KEY_KP_SUBTRACT;
    case gfx::Key::PageUp:
        return KEY_PAGE_UP;
    case gfx::Key::PageDown:
        return KEY_PAGE_DOWN;
    case gfx::Key::End:
        return KEY_END;
    case gfx::Key::Tab:
        return KEY_TAB;
    case gfx::Key::F1:
        return KEY_F1;
    case gfx::Key::F3:
        return KEY_F3;
    }
    return KEY_NULL;
}

int toRlPadBtn(gfx::PadBtn b)
{
    switch (b)
    {
    case gfx::PadBtn::FaceDown:
        return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
    case gfx::PadBtn::FaceRight:
        return GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
    case gfx::PadBtn::FaceLeft:
        return GAMEPAD_BUTTON_RIGHT_FACE_LEFT;
    case gfx::PadBtn::FaceUp:
        return GAMEPAD_BUTTON_RIGHT_FACE_UP;
    case gfx::PadBtn::LeftBumper:
        return GAMEPAD_BUTTON_LEFT_TRIGGER_1;
    case gfx::PadBtn::RightBumper:
        return GAMEPAD_BUTTON_RIGHT_TRIGGER_1;
    case gfx::PadBtn::Select:
        return GAMEPAD_BUTTON_MIDDLE_LEFT;
    case gfx::PadBtn::Start:
        return GAMEPAD_BUTTON_MIDDLE_RIGHT;
    case gfx::PadBtn::LeftThumb:
        return GAMEPAD_BUTTON_LEFT_THUMB;
    case gfx::PadBtn::RightThumb:
        return GAMEPAD_BUTTON_RIGHT_THUMB;
    case gfx::PadBtn::DpadUp:
        return GAMEPAD_BUTTON_LEFT_FACE_UP;
    case gfx::PadBtn::DpadDown:
        return GAMEPAD_BUTTON_LEFT_FACE_DOWN;
    case gfx::PadBtn::DpadLeft:
        return GAMEPAD_BUTTON_LEFT_FACE_LEFT;
    case gfx::PadBtn::DpadRight:
        return GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
    }
    return GAMEPAD_BUTTON_UNKNOWN;
}

int toRlPadAxis(gfx::PadAxis a)
{
    switch (a)
    {
    case gfx::PadAxis::LeftX:
        return GAMEPAD_AXIS_LEFT_X;
    case gfx::PadAxis::LeftY:
        return GAMEPAD_AXIS_LEFT_Y;
    case gfx::PadAxis::RightX:
        return GAMEPAD_AXIS_RIGHT_X;
    case gfx::PadAxis::RightY:
        return GAMEPAD_AXIS_RIGHT_Y;
    case gfx::PadAxis::LeftTrigger:
        return GAMEPAD_AXIS_LEFT_TRIGGER;
    case gfx::PadAxis::RightTrigger:
        return GAMEPAD_AXIS_RIGHT_TRIGGER;
    }
    return GAMEPAD_AXIS_LEFT_X;
}

} // namespace

// ---------------------------------------------------------------------------
// gfx logging
// ---------------------------------------------------------------------------
namespace gfx
{

void logWarn(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    TraceLog(LOG_WARNING, "%s", buf);
}

void logInfo(const char *fmt, ...)
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
struct RaylibEngine::Impl
{
    struct AnimSet
    {
        ModelAnimation *anims{nullptr};
        int count{0};
    };

    std::vector<Model> models;
    std::vector<Texture2D> textures;
    std::vector<Music> musics;
    std::vector<AnimSet> animSets;

    // Skybox (a unit cube + equirectangular shader, drawn camera-centred).
    Model skybox{};
    Texture2D skyboxTex{};
    Shader skyShader{};
    bool skyboxLoaded{false};

    // UI font, used by drawText/measureText when loaded (built-in otherwise).
    Font uiFont{};
    bool uiFontLoaded{false};

    // Per-pixel lighting applied to every model's materials.
    Shader lightShader{};
    bool lightLoaded{false};
    int lightViewPosLoc{-1};

    // Instanced variant of the lighting shader (model matrix as attribute),
    // swapped in for the duration of a drawModelInstanced call.
    Shader instShader{};
    bool instLoaded{false};
    int instViewPosLoc{-1};

    // Bloom post FX: the 3D scene renders into full-res sceneRT; endMode3D
    // extracts+blurs the bright parts into half-res bloomRT (quarter of the
    // heavy taps), then composites both to the screen.
    Shader bloomExtract{};
    Shader bloomCombine{};
    bool bloomLoaded{false};
    int bloomResLoc{-1};    // "resolution" on the extract shader
    int bloomGlowLoc{-1};   // "glowTex" sampler on the combine shader
    RenderTexture2D sceneRT{};
    RenderTexture2D bloomRT{}; // half of sceneRT
    bool sceneRTActive{false};

    bool valid(int h, std::size_t n) const
    {
        return h >= 0 && static_cast<std::size_t>(h) < n;
    }
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
RaylibEngine::RaylibEngine(int width, int height, const std::string &title) : _impl(std::make_unique<Impl>())
{
    InitWindow(width, height, title.c_str());
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
}

RaylibEngine::~RaylibEngine()
{
    // Free any GPU resources still loaded while the GL context is alive.
    unloadSkybox();
    unloadUiFont();
    if (_impl->lightLoaded)
        UnloadShader(_impl->lightShader);
    if (_impl->instLoaded)
        UnloadShader(_impl->instShader);
    if (_impl->bloomLoaded)
    {
        UnloadShader(_impl->bloomExtract);
        UnloadShader(_impl->bloomCombine);
        UnloadRenderTexture(_impl->sceneRT);
        UnloadRenderTexture(_impl->bloomRT);
    }
    for (auto &s : _impl->animSets)
        if (s.anims)
            UnloadModelAnimations(s.anims, s.count);
    for (auto &m : _impl->models)
        if (m.meshCount > 0)
            UnloadModel(m);
    for (auto &t : _impl->textures)
        if (t.id != 0)
            UnloadTexture(t);
    for (auto &mu : _impl->musics)
        if (mu.stream.buffer)
            UnloadMusicStream(mu);
    CloseWindow();
}

bool RaylibEngine::shouldClose() const
{
    return WindowShouldClose();
}

void RaylibEngine::beginDrawing()
{
    BeginDrawing();
    // The 360 background is a skybox drawn inside the 3D scene, so just clear.
    ClearBackground(BLACK);
}

void RaylibEngine::endDrawing()
{
    EndDrawing();
}
void RaylibEngine::disableCursor()
{
    DisableCursor();
}
void RaylibEngine::enableCursor()
{
    EnableCursor();
}
int RaylibEngine::screenWidth() const
{
    return GetScreenWidth();
}
int RaylibEngine::screenHeight() const
{
    return GetScreenHeight();
}
void RaylibEngine::drawFps(int x, int y)
{
    // Not DrawFPS(): that always uses the built-in font, which would make the
    // FPS counter the one label ignoring the loaded UI font.
    drawText(TextFormat("%2i FPS", GetFPS()), x, y, 20, gfx::GREEN);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
bool RaylibEngine::keyDown(gfx::Key k) const
{
    return IsKeyDown(toRlKey(k));
}
bool RaylibEngine::keyPressed(gfx::Key k) const
{
    return IsKeyPressed(toRlKey(k));
}
int RaylibEngine::charPressed() const
{
    return GetCharPressed();
}
bool RaylibEngine::mousePressed(gfx::MouseBtn) const
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
gfx::Vec2 RaylibEngine::mouseDelta() const
{
    return fromRl(GetMouseDelta());
}
float RaylibEngine::mouseWheel() const
{
    return GetMouseWheelMove();
}
bool RaylibEngine::padAvailable() const
{
    return IsGamepadAvailable(0);
}
bool RaylibEngine::padDown(gfx::PadBtn b) const
{
    return IsGamepadButtonDown(0, toRlPadBtn(b));
}
bool RaylibEngine::padPressed(gfx::PadBtn b) const
{
    return IsGamepadButtonPressed(0, toRlPadBtn(b));
}
float RaylibEngine::padAxis(gfx::PadAxis a) const
{
    return GetGamepadAxisMovement(0, toRlPadAxis(a));
}
double RaylibEngine::time() const
{
    return GetTime();
}
float RaylibEngine::frameTime() const
{
    return GetFrameTime();
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------
bool RaylibEngine::initAudio()
{
    InitAudioDevice();
    return IsAudioDeviceReady();
}

void RaylibEngine::closeAudio()
{
    CloseAudioDevice();
}

gfx::MusicHandle RaylibEngine::loadMusic(const std::string &path)
{
    Music m = LoadMusicStream(path.c_str());
    if (m.stream.buffer == nullptr)
        return gfx::NoHandle;
    m.looping = true;
    _impl->musics.push_back(m);
    return static_cast<gfx::MusicHandle>(_impl->musics.size() - 1);
}

void RaylibEngine::playMusic(gfx::MusicHandle h)
{
    if (_impl->valid(h, _impl->musics.size()))
        PlayMusicStream(_impl->musics[h]);
}
void RaylibEngine::stopMusic(gfx::MusicHandle h)
{
    if (_impl->valid(h, _impl->musics.size()))
        StopMusicStream(_impl->musics[h]);
}
void RaylibEngine::pauseMusic(gfx::MusicHandle h)
{
    if (_impl->valid(h, _impl->musics.size()))
        PauseMusicStream(_impl->musics[h]);
}
void RaylibEngine::resumeMusic(gfx::MusicHandle h)
{
    if (_impl->valid(h, _impl->musics.size()))
        ResumeMusicStream(_impl->musics[h]);
}
void RaylibEngine::updateMusic(gfx::MusicHandle h)
{
    if (_impl->valid(h, _impl->musics.size()))
        UpdateMusicStream(_impl->musics[h]);
}
void RaylibEngine::setMusicVolume(gfx::MusicHandle h, float v)
{
    if (_impl->valid(h, _impl->musics.size()))
        SetMusicVolume(_impl->musics[h], v);
}

void RaylibEngine::unloadMusic(gfx::MusicHandle h)
{
    if (_impl->valid(h, _impl->musics.size()) && _impl->musics[h].stream.buffer)
    {
        UnloadMusicStream(_impl->musics[h]);
        _impl->musics[h] = {};
    }
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------
gfx::TextureHandle RaylibEngine::loadTexture(const std::string &path)
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
    if (_impl->valid(h, _impl->textures.size()) && _impl->textures[h].id != 0)
    {
        UnloadTexture(_impl->textures[h]);
        _impl->textures[h] = {};
    }
}

// ---------------------------------------------------------------------------
// Models
// ---------------------------------------------------------------------------
gfx::ModelHandle RaylibEngine::loadModel(const std::string &path)
{
    Model m = LoadModel(path.c_str());
    if (m.meshCount <= 0)
    {
        if (m.meshCount == 0)
            UnloadModel(m); // free any partial allocation
        return gfx::NoHandle;
    }
    // Route every material through the lighting shader (glTF puts the real
    // materials at 1+, with a raylib default at 0 — cover them all).
    if (_impl->lightLoaded)
        for (int i = 0; i < m.materialCount; ++i)
            m.materials[i].shader = _impl->lightShader;
    _impl->models.push_back(m);
    return static_cast<gfx::ModelHandle>(_impl->models.size() - 1);
}

bool RaylibEngine::loadLightingShader(const std::string &vs, const std::string &fs)
{
    if (!FileExists(vs.c_str()) || !FileExists(fs.c_str()))
    {
        TraceLog(LOG_WARNING, "loadLightingShader: missing shader files — flat shading kept");
        return false;
    }
    Shader s = LoadShader(vs.c_str(), fs.c_str());
    if (s.id == 0)
    {
        TraceLog(LOG_WARNING, "loadLightingShader: compile failed — flat shading kept");
        return false;
    }
    _impl->lightShader = s;
    _impl->lightViewPosLoc = GetShaderLocation(s, "viewPos");
    _impl->lightLoaded = true;

    // Retro-apply to models loaded before the shader (robust to call order).
    for (auto &m : _impl->models)
        for (int i = 0; i < m.materialCount; ++i)
            m.materials[i].shader = s;

    TraceLog(LOG_INFO, "loadLightingShader: '%s' ready", fs.c_str());
    return true;
}

bool RaylibEngine::loadInstancingShader(const std::string &vs, const std::string &fs)
{
    if (!FileExists(vs.c_str()) || !FileExists(fs.c_str()))
    {
        TraceLog(LOG_WARNING, "loadInstancingShader: missing shader files — per-instance draws kept");
        return false;
    }
    Shader s = LoadShader(vs.c_str(), fs.c_str());
    if (s.id == 0)
    {
        TraceLog(LOG_WARNING, "loadInstancingShader: compile failed — per-instance draws kept");
        return false;
    }
    // DrawMeshInstanced reads the model matrix from this vertex attribute.
    s.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(s, "instanceTransform");
    _impl->instShader = s;
    _impl->instViewPosLoc = GetShaderLocation(s, "viewPos");
    _impl->instLoaded = true;
    TraceLog(LOG_INFO, "loadInstancingShader: '%s' ready", vs.c_str());
    return true;
}

bool RaylibEngine::enableBloom(const std::string &extractFs, const std::string &combineFs)
{
    if (!FileExists(extractFs.c_str()) || !FileExists(combineFs.c_str()))
    {
        TraceLog(LOG_WARNING, "enableBloom: missing shader files — no post FX");
        return false;
    }
    Shader ext = LoadShader(nullptr, extractFs.c_str()); // default fullscreen vs
    if (ext.id == 0)
    {
        TraceLog(LOG_WARNING, "enableBloom: extract shader failed to compile — no post FX");
        return false;
    }
    Shader comb = LoadShader(nullptr, combineFs.c_str());
    if (comb.id == 0)
    {
        TraceLog(LOG_WARNING, "enableBloom: combine shader failed to compile — no post FX");
        UnloadShader(ext);
        return false;
    }
    _impl->sceneRT = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    _impl->bloomRT = LoadRenderTexture(GetScreenWidth() / 2, GetScreenHeight() / 2);
    if (_impl->sceneRT.texture.id == 0 || _impl->bloomRT.texture.id == 0)
    {
        TraceLog(LOG_WARNING, "enableBloom: render texture failed — no post FX");
        UnloadShader(ext);
        UnloadShader(comb);
        return false;
    }
    // Bilinear so the half-res glow upscales smoothly instead of blocky.
    SetTextureFilter(_impl->bloomRT.texture, TEXTURE_FILTER_BILINEAR);
    _impl->bloomExtract = ext;
    _impl->bloomCombine = comb;
    _impl->bloomResLoc = GetShaderLocation(ext, "resolution");
    _impl->bloomGlowLoc = GetShaderLocation(comb, "glowTex");
    _impl->bloomLoaded = true;
    TraceLog(LOG_INFO, "enableBloom: '%s' + '%s' ready (half-res glow)", extractFs.c_str(), combineFs.c_str());
    return true;
}

gfx::BBox RaylibEngine::modelBounds(gfx::ModelHandle h) const
{
    if (!_impl->valid(h, _impl->models.size()))
        return {};
    BoundingBox b = GetModelBoundingBox(_impl->models[h]);
    return {fromRl(b.min), fromRl(b.max)};
}

void RaylibEngine::translateModel(gfx::ModelHandle h, gfx::Vec3 t)
{
    if (!_impl->valid(h, _impl->models.size()))
        return;
    _impl->models[h].transform = MatrixMultiply(_impl->models[h].transform, MatrixTranslate(t.x, t.y, t.z));
}

void RaylibEngine::bindModelTexture(gfx::ModelHandle h, gfx::TextureHandle tex)
{
    if (!_impl->valid(h, _impl->models.size()) || !_impl->valid(tex, _impl->textures.size()))
        return;
    Model &m = _impl->models[h];
    // raylib prepends a default material at index 0 and puts the real glTF
    // materials at 1+, so apply to all of them (binding only [0] misses the mesh's).
    for (int i = 0; i < m.materialCount; ++i)
        SetMaterialTexture(&m.materials[i], MATERIAL_MAP_DIFFUSE, _impl->textures[tex]);
}

void RaylibEngine::unloadModel(gfx::ModelHandle h)
{
    if (_impl->valid(h, _impl->models.size()) && _impl->models[h].meshCount > 0)
    {
        UnloadModel(_impl->models[h]);
        _impl->models[h] = {};
    }
}

// ---------------------------------------------------------------------------
// Model animations
// ---------------------------------------------------------------------------
gfx::AnimSetHandle RaylibEngine::loadAnimations(const std::string &path)
{
    int count = 0;
    ModelAnimation *a = LoadModelAnimations(path.c_str(), &count);
    if (!a || count <= 0)
        return gfx::NoHandle;
    _impl->animSets.push_back({a, count});
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
    if (_impl->valid(s, _impl->animSets.size()) && _impl->animSets[s].anims)
    {
        UnloadModelAnimations(_impl->animSets[s].anims, _impl->animSets[s].count);
        _impl->animSets[s] = {};
    }
}

// ---------------------------------------------------------------------------
// 3D drawing
// ---------------------------------------------------------------------------
void RaylibEngine::beginMode3D(const gfx::Camera &cam)
{
    if (_impl->bloomLoaded)
    {
        // Track window size so the offscreen targets never stretch.
        if (_impl->sceneRT.texture.width != GetScreenWidth() || _impl->sceneRT.texture.height != GetScreenHeight())
        {
            UnloadRenderTexture(_impl->sceneRT);
            UnloadRenderTexture(_impl->bloomRT);
            _impl->sceneRT = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
            _impl->bloomRT = LoadRenderTexture(GetScreenWidth() / 2, GetScreenHeight() / 2);
            SetTextureFilter(_impl->bloomRT.texture, TEXTURE_FILTER_BILINEAR);
        }
        BeginTextureMode(_impl->sceneRT);
        ClearBackground(BLACK);
        _impl->sceneRTActive = true;
    }
    const Vector3 eye = toRl(cam.position);
    if (_impl->lightLoaded && _impl->lightViewPosLoc >= 0)
        SetShaderValue(_impl->lightShader, _impl->lightViewPosLoc, &eye, SHADER_UNIFORM_VEC3);
    if (_impl->instLoaded && _impl->instViewPosLoc >= 0)
        SetShaderValue(_impl->instShader, _impl->instViewPosLoc, &eye, SHADER_UNIFORM_VEC3);
    BeginMode3D(toRlCamera(cam));
}
void RaylibEngine::endMode3D()
{
    EndMode3D();
    if (_impl->sceneRTActive)
    {
        EndTextureMode();
        _impl->sceneRTActive = false;
        const float res[2] = {static_cast<float>(_impl->sceneRT.texture.width),
                              static_cast<float>(_impl->sceneRT.texture.height)};

        // Pass 1: bright-extract + hex blur into the half-res glow target.
        // (Negative source height: render textures are stored bottom-up; the
        // flip keeps bloomRT in the same texel orientation as sceneRT so the
        // combine pass can sample both with one set of coords.)
        if (_impl->bloomResLoc >= 0)
            SetShaderValue(_impl->bloomExtract, _impl->bloomResLoc, res, SHADER_UNIFORM_VEC2);
        BeginTextureMode(_impl->bloomRT);
        BeginShaderMode(_impl->bloomExtract);
        DrawTexturePro(_impl->sceneRT.texture, Rectangle{0.0f, 0.0f, res[0], -res[1]},
                       Rectangle{0.0f, 0.0f, static_cast<float>(_impl->bloomRT.texture.width),
                                 static_cast<float>(_impl->bloomRT.texture.height)},
                       Vector2{0.0f, 0.0f}, 0.0f, WHITE);
        EndShaderMode();
        EndTextureMode();

        // Pass 2: composite the scene with the upscaled glow.
        BeginShaderMode(_impl->bloomCombine);
        if (_impl->bloomGlowLoc >= 0)
            SetShaderValueTexture(_impl->bloomCombine, _impl->bloomGlowLoc, _impl->bloomRT.texture);
        DrawTextureRec(_impl->sceneRT.texture, Rectangle{0.0f, 0.0f, res[0], -res[1]}, Vector2{0.0f, 0.0f}, WHITE);
        EndShaderMode();
    }
}

void RaylibEngine::drawModelEx(gfx::ModelHandle h, gfx::Vec3 pos, gfx::Vec3 axis, float angleDeg, gfx::Vec3 scale,
                               gfx::Color tint)
{
    if (_impl->valid(h, _impl->models.size()) && _impl->models[h].meshCount > 0)
        DrawModelEx(_impl->models[h], toRl(pos), toRl(axis), angleDeg, toRl(scale), toRl(tint));
}

namespace
{
// scale -> yaw about local up -> orthonormal basis + translation. Column-major
// basis: local X/Y/Z land on right/up/back. raymath MatrixMultiply(A, B)
// applies A first.
Matrix composeWorld(gfx::Vec3 pos, gfx::Vec3 right, gfx::Vec3 up, gfx::Vec3 back, float yawDeg, gfx::Vec3 scale)
{
    Matrix basis = MatrixIdentity();
    basis.m0 = right.x;
    basis.m1 = right.y;
    basis.m2 = right.z;
    basis.m4 = up.x;
    basis.m5 = up.y;
    basis.m6 = up.z;
    basis.m8 = back.x;
    basis.m9 = back.y;
    basis.m10 = back.z;
    basis.m12 = pos.x;
    basis.m13 = pos.y;
    basis.m14 = pos.z;
    return MatrixMultiply(MatrixMultiply(MatrixScale(scale.x, scale.y, scale.z), MatrixRotateY(DEG2RAD * yawDeg)),
                          basis);
}
} // namespace

void RaylibEngine::drawModelOriented(gfx::ModelHandle h, gfx::Vec3 pos, gfx::Vec3 right, gfx::Vec3 up, gfx::Vec3 back,
                                     float yawDeg, gfx::Vec3 scale, gfx::Color tint)
{
    if (!_impl->valid(h, _impl->models.size()) || _impl->models[h].meshCount == 0)
        return;
    Model &model = _impl->models[h];

    // Splice under the baked transform for one draw; DrawModel then only adds
    // an identity, so the composed matrix fully controls placement.
    const Matrix saved = model.transform;
    model.transform = MatrixMultiply(saved, composeWorld(pos, right, up, back, yawDeg, scale));
    DrawModel(model, Vector3{0.0f, 0.0f, 0.0f}, 1.0f, toRl(tint));
    model.transform = saved;
}

void RaylibEngine::drawModelInstanced(gfx::ModelHandle h, const std::vector<gfx::InstanceXform> &xforms,
                                      gfx::Color tint)
{
    if (xforms.empty() || !_impl->valid(h, _impl->models.size()) || _impl->models[h].meshCount == 0)
        return;

    if (!_impl->instLoaded)
    {
        // No instancing shader: same result, one draw per placement.
        for (const auto &x : xforms)
            drawModelOriented(h, x.pos, x.right, x.up, x.back, x.yawDeg, x.scale, tint);
        return;
    }

    Model &m = _impl->models[h];
    std::vector<Matrix> mats;
    mats.reserve(xforms.size());
    for (const auto &x : xforms)
        mats.push_back(MatrixMultiply(m.transform, composeWorld(x.pos, x.right, x.up, x.back, x.yawDeg, x.scale)));

    const Color tc = toRl(tint);
    for (int i = 0; i < m.meshCount; ++i)
    {
        // Swap in the instancing shader (and tint) just for this call.
        Material &mat = m.materials[m.meshMaterial[i]];
        const Shader savedShader = mat.shader;
        const Color savedColor = mat.maps[MATERIAL_MAP_DIFFUSE].color;
        mat.shader = _impl->instShader;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = tc;
        DrawMeshInstanced(m.meshes[i], mat, mats.data(), static_cast<int>(mats.size()));
        mat.shader = savedShader;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = savedColor;
    }
}

void RaylibEngine::drawCube(gfx::Vec3 pos, float w, float h, float l, gfx::Color c)
{
    DrawCube(toRl(pos), w, h, l, toRl(c));
}
void RaylibEngine::drawPlane(gfx::Vec3 center, gfx::Vec2 size, gfx::Color c)
{
    DrawPlane(toRl(center), toRl(size), toRl(c));
}
void RaylibEngine::drawSphere(gfx::Vec3 center, float radius, gfx::Color c)
{
    DrawSphere(toRl(center), radius, toRl(c));
}
void RaylibEngine::drawLine3D(gfx::Vec3 a, gfx::Vec3 b, gfx::Color c)
{
    DrawLine3D(toRl(a), toRl(b), toRl(c));
}
void RaylibEngine::drawCircle3D(gfx::Vec3 center, float radius, gfx::Color c)
{
    // Rotated 90 deg around X so the circle lies flat on the ground plane.
    DrawCircle3D(toRl(center), radius, Vector3{1.0f, 0.0f, 0.0f}, 90.0f, toRl(c));
}
void RaylibEngine::drawCircle3D(gfx::Vec3 center, float radius, gfx::Vec3 normal, gfx::Color c)
{
    // DrawCircle3D's base circle lies in the XY plane (normal +Z); rotate +Z
    // onto the requested normal.
    const Vector3 n = Vector3Normalize(toRl(normal));
    const Vector3 z = {0.0f, 0.0f, 1.0f};
    Vector3 axis = Vector3CrossProduct(z, n);
    const float len = Vector3Length(axis);
    if (len < 1e-6f)
        axis = {1.0f, 0.0f, 0.0f}; // normal (anti)parallel to +Z: any perpendicular works
    const float angle = RAD2DEG * atan2f(len, Vector3DotProduct(z, n));
    DrawCircle3D(toRl(center), radius, axis, angle, toRl(c));
}
void RaylibEngine::drawQuad3D(gfx::Vec3 a, gfx::Vec3 b, gfx::Vec3 c, gfx::Vec3 d, gfx::Color col)
{
    // Both windings so the quad reads from either side (torus underside).
    DrawTriangle3D(toRl(a), toRl(b), toRl(c), toRl(col));
    DrawTriangle3D(toRl(a), toRl(c), toRl(d), toRl(col));
    DrawTriangle3D(toRl(c), toRl(b), toRl(a), toRl(col));
    DrawTriangle3D(toRl(d), toRl(c), toRl(a), toRl(col));
}

void RaylibEngine::drawTexturedQuad3D(gfx::TextureHandle tex, gfx::Vec3 a, gfx::Vec3 b, gfx::Vec3 c, gfx::Vec3 d,
                                      gfx::Vec3 normal, gfx::Color tint)
{
    if (!_impl->valid(tex, _impl->textures.size()))
        return;

    const auto emit = [&](gfx::Vec3 p, float u, float v) {
        rlTexCoord2f(u, v);
        rlVertex3f(p.x, p.y, p.z);
    };

    rlSetTexture(_impl->textures[tex].id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlNormal3f(normal.x, normal.y, normal.z);
    emit(a, 0.0f, 1.0f);
    emit(b, 1.0f, 1.0f);
    emit(c, 1.0f, 0.0f);
    emit(d, 0.0f, 0.0f);
    emit(d, 0.0f, 0.0f);
    emit(c, 1.0f, 0.0f);
    emit(b, 1.0f, 1.0f);
    emit(a, 0.0f, 1.0f);
    rlEnd();
    rlSetTexture(0);
}

gfx::Vec2 RaylibEngine::worldToScreen(const gfx::Camera &cam, gfx::Vec3 world) const
{
    return fromRl(GetWorldToScreen(toRl(world), toRlCamera(cam)));
}

gfx::Ray RaylibEngine::screenToWorldRay(const gfx::Camera &cam, gfx::Vec2 screen) const
{
    Ray r = GetScreenToWorldRay(toRl(screen), toRlCamera(cam));
    return {fromRl(r.position), fromRl(r.direction)};
}

// ---------------------------------------------------------------------------
// 2D drawing
// ---------------------------------------------------------------------------
bool RaylibEngine::loadUiFont(const std::string &path, int baseSize)
{
    unloadUiFont();
    // Rasterize the default glyph set at a size above every UI size we draw
    // (14-34px), so text only ever downscales; bilinear keeps that smooth.
    Font f = LoadFontEx(path.c_str(), baseSize, nullptr, 0);
    if (f.texture.id == 0 || f.glyphCount <= 0)
        return false;
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    _impl->uiFont = f;
    _impl->uiFontLoaded = true;
    return true;
}

void RaylibEngine::unloadUiFont()
{
    if (_impl->uiFontLoaded)
    {
        UnloadFont(_impl->uiFont);
        _impl->uiFont = {};
        _impl->uiFontLoaded = false;
    }
}

namespace
{
// DrawText's internal letter spacing is fontSize / defaultFontSize (10); use
// the same ratio for the custom font so swapping fonts never reflows the UI
// more than the glyph shapes themselves do.
float uiSpacing(int fontSize)
{
    return static_cast<float>(fontSize) / 10.0f;
}
} // namespace

void RaylibEngine::drawText(const std::string &text, int x, int y, int fontSize, gfx::Color c)
{
    if (_impl->uiFontLoaded)
    {
        DrawTextEx(_impl->uiFont, text.c_str(), {static_cast<float>(x), static_cast<float>(y)},
                   static_cast<float>(fontSize), uiSpacing(fontSize), toRl(c));
        return;
    }
    DrawText(text.c_str(), x, y, fontSize, toRl(c));
}
int RaylibEngine::measureText(const std::string &text, int fontSize) const
{
    if (_impl->uiFontLoaded)
        return static_cast<int>(
            MeasureTextEx(_impl->uiFont, text.c_str(), static_cast<float>(fontSize), uiSpacing(fontSize)).x);
    return MeasureText(text.c_str(), fontSize);
}
void RaylibEngine::drawRect(int x, int y, int w, int h, gfx::Color c)
{
    DrawRectangle(x, y, w, h, toRl(c));
}
void RaylibEngine::drawRectLines(int x, int y, int w, int h, gfx::Color c)
{
    DrawRectangleLines(x, y, w, h, toRl(c));
}
void RaylibEngine::drawLine(int x1, int y1, int x2, int y2, gfx::Color c)
{
    DrawLine(x1, y1, x2, y2, toRl(c));
}
void RaylibEngine::drawCircle(int cx, int cy, float radius, gfx::Color c)
{
    DrawCircle(cx, cy, radius, toRl(c));
}

// ---------------------------------------------------------------------------
// Skybox (owns the cube + shader + texture and the GL state toggles)
// ---------------------------------------------------------------------------
bool RaylibEngine::loadSkybox(const std::string &png, const std::string &vs, const std::string &fs)
{
    if (!FileExists(png.c_str()))
    {
        TraceLog(LOG_WARNING, "loadSkybox: missing '%s' — no background", png.c_str());
        return false;
    }
    if (!FileExists(vs.c_str()) || !FileExists(fs.c_str()))
    {
        TraceLog(LOG_WARNING, "loadSkybox: missing shader files — no background");
        return false;
    }
    _impl->skyboxTex = LoadTexture(png.c_str());
    if (_impl->skyboxTex.id == 0)
    {
        TraceLog(LOG_WARNING, "loadSkybox: failed to load '%s'", png.c_str());
        return false;
    }
    SetTextureFilter(_impl->skyboxTex, TEXTURE_FILTER_BILINEAR);

    _impl->skyShader = LoadShader(vs.c_str(), fs.c_str());
    if (_impl->skyShader.id == 0)
    {
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
    DrawModel(_impl->skybox, Vector3{0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

void RaylibEngine::unloadSkybox()
{
    if (_impl->skyboxLoaded)
    {
        UnloadModel(_impl->skybox); // does not free the texture map we assigned
        UnloadShader(_impl->skyShader);
    }
    if (_impl->skyboxTex.id != 0)
        UnloadTexture(_impl->skyboxTex);
    _impl->skyboxTex = {};
    _impl->skyboxLoaded = false;
}

// ---------------------------------------------------------------------------
// Batched checkerboard floor (one rlBegin(RL_QUADS) per colour, chunked)
// ---------------------------------------------------------------------------
namespace
{
void drawFloorBatch(Texture2D tex, bool wantDark, int w, int h, float tileSize, float quadSize, float surfaceY)
{
    if (tex.id == 0)
        return;

    const float half = quadSize * 0.5f;
    const float y = surfaceY;
    const int kQuadsPerBatch = 1500; // 4 verts each, well under the 8192 buffer

    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlNormal3f(0.0f, 1.0f, 0.0f);

    int emitted = 0;
    for (int ty = 0; ty < h; ++ty)
    {
        for (int tx = 0; tx < w; ++tx)
        {
            if (((tx + ty) % 2 == 0) != wantDark)
                continue;
            if (emitted >= kQuadsPerBatch)
            {
                rlEnd(); // flush, then resume (texture stays bound across this)
                rlBegin(RL_QUADS);
                rlColor4ub(255, 255, 255, 255);
                rlNormal3f(0.0f, 1.0f, 0.0f);
                emitted = 0;
            }
            float wx = tx * tileSize + tileSize / 2.0f;
            float wz = ty * tileSize + tileSize / 2.0f;
            rlTexCoord2f(0.0f, 1.0f);
            rlVertex3f(wx - half, y, wz - half);
            rlTexCoord2f(0.0f, 0.0f);
            rlVertex3f(wx - half, y, wz + half);
            rlTexCoord2f(1.0f, 0.0f);
            rlVertex3f(wx + half, y, wz + half);
            rlTexCoord2f(1.0f, 1.0f);
            rlVertex3f(wx + half, y, wz - half);
            ++emitted;
        }
    }

    rlEnd();
    rlSetTexture(0);
}
} // namespace

void RaylibEngine::drawCheckerFloor(gfx::TextureHandle dark, gfx::TextureHandle light, int w, int h, float tileSize,
                                    float quadSize, float surfaceY)
{
    if (_impl->valid(dark, _impl->textures.size()))
        drawFloorBatch(_impl->textures[dark], true, w, h, tileSize, quadSize, surfaceY);
    if (_impl->valid(light, _impl->textures.size()))
        drawFloorBatch(_impl->textures[light], false, w, h, tileSize, quadSize, surfaceY);
}
