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
    case gfx::Key::Q:
        return KEY_Q;
    case gfx::Key::R:
        return KEY_R;
    case gfx::Key::F:
        return KEY_F;
    case gfx::Key::G:
        return KEY_G;
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
    case gfx::Key::F5:
        return KEY_F5;
    case gfx::Key::F6:
        return KEY_F6;
    case gfx::Key::F7:
        return KEY_F7;
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

namespace
{
// Locations des uniforms scène partagés par les shaders light / inst / floor.
struct SceneLocs
{
    int sunDir{-1}, sunColor{-1}, ambient{-1}, fogColor{-1}, fogDensity{-1};
};

// Locations des uniforms d'ombre par shader (lighting / instancing / floor).
struct ShadowLocs
{
    int map{-1}, vp{-1}, on{-1}, res{-1};
};
} // namespace

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

    // Ciel procédural : locations des uniforms de sky_procedural.fs.
    struct SkyLocs
    {
        int time{-1}, toSun{-1}, toMoon{-1}, sunGlow{-1}, horizon{-1}, zenith{-1}, dayTint{-1}, nebula{-1}, stars{-1},
            aurora{-1},
            lightning{-1};
    };
    SkyLocs skyLocs{};
    bool skyProcedural{false};

    Shader celestialShader{};
    bool celestialLoaded{false};
    int celEmissiveLoc{-1}, celAlphaLoc{-1}, celMoonLoc{-1}, celTimeLoc{-1};

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

    // Scene lighting/fog uniforms shared by the light / inst / floor shaders,
    // plus the dedicated batched-floor shader (seasonal ground tint).
    SceneLocs lightScene{}, instScene{}, floorScene{};
    Shader floorShader{};
    bool floorLoaded{false};
    int floorViewPosLoc{-1};
    int floorOverlayLoc{-1}, floorMixLoc{-1};
    // Carte d'accumulation au sol (neige/feuilles/eau/traces) : sampler + infos.
    int floorCoverLoc{-1}, floorCoverOnLoc{-1}, floorWorldSizeLoc{-1}, floorTileSizeLoc{-1}, floorTimeLoc{-1};

    // Shadow map (FBO depth-only) + locations par shader.
    RenderTexture2D shadowRT{};
    bool shadowsLoaded{false};
    int shadowRes{0};
    Matrix lightVP{};
    ShadowLocs lightShadow{}, instShadow{}, floorShadow{};

    // Bloom post FX: the 3D scene renders into full-res sceneRT; endMode3D
    // extracts+blurs the bright parts into half-res bloomRT (quarter of the
    // heavy taps), then composites both to the screen.
    Shader bloomExtract{};
    Shader bloomCombine{};
    bool bloomLoaded{false};
    int bloomResLoc{-1};    // "resolution" on the extract shader
    RenderTexture2D sceneRT{};
    RenderTexture2D bloomRT{}; // half of sceneRT
    bool sceneRTActive{false};
    RenderTexture2D previewRT{};
    int previewW{0};
    int previewH{0};

    // Locations du composite + valeurs par frame (appliquées dans endMode3D).
    struct PostLocs
    {
        int glowTex{-1}, sunScreen{-1}, godray{-1}, lift{-1}, gain{-1}, heat{-1}, time{-1};
    };
    PostLocs postLocs{};
    struct PostParams
    {
        Vector2 sunScreen{0.5f, 0.5f};
        float godray{0};
        Vector3 lift{0, 0, 0};
        Vector3 gain{1, 1, 1};
        float heat{0};
        float time{0};
    };
    PostParams post{};

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
    // Far plane généreux : la carte fait ~1600 unités de côté et la caméra
    // libre monte haut — le défaut (4000) coupait les coins en vue lointaine.
    rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, 12000.0);
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
    if (_impl->floorLoaded)
        UnloadShader(_impl->floorShader);
    if (_impl->celestialLoaded)
        UnloadShader(_impl->celestialShader);
    if (_impl->shadowsLoaded)
        UnloadRenderTexture(_impl->shadowRT);
    if (_impl->bloomLoaded)
    {
        UnloadShader(_impl->bloomExtract);
        UnloadShader(_impl->bloomCombine);
        UnloadRenderTexture(_impl->sceneRT);
        UnloadRenderTexture(_impl->bloomRT);
    }
    if (_impl->previewRT.id != 0)
        UnloadRenderTexture(_impl->previewRT);
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
void RaylibEngine::toggleFullscreen()
{
    // Borderless rather than exclusive fullscreen: no video-mode switch, so
    // alt-tab stays instant and the post-FX render targets just follow the
    // next GetScreenWidth/Height reading.
    ToggleBorderlessWindowed();
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
gfx::Vec2 RaylibEngine::mousePosition() const
{
    return fromRl(GetMousePosition());
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

gfx::TextureHandle RaylibEngine::createDataTexture(int w, int h)
{
    Image img = GenImageColor(w, h, BLANK);
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    if (t.id == 0)
        return gfx::NoHandle;
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT); // la carte boucle (tore)
    _impl->textures.push_back(t);
    return static_cast<gfx::TextureHandle>(_impl->textures.size() - 1);
}

void RaylibEngine::updateTexture(gfx::TextureHandle h, const std::uint8_t *rgba)
{
    if (_impl->valid(h, _impl->textures.size()) && _impl->textures[h].id != 0)
        UpdateTexture(_impl->textures[h], rgba);
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

namespace
{
SceneLocs cacheSceneLocs(Shader s)
{
    SceneLocs l;
    l.sunDir = GetShaderLocation(s, "sunDir");
    l.sunColor = GetShaderLocation(s, "sunColor");
    l.ambient = GetShaderLocation(s, "ambientColor");
    l.fogColor = GetShaderLocation(s, "fogColor");
    l.fogDensity = GetShaderLocation(s, "fogDensity");
    return l;
}

void applySceneUniforms(Shader s, const SceneLocs &l, Vector3 dir, Vector3 col, Vector3 amb,
                        Vector3 fog, float fogD)
{
    if (l.sunDir >= 0)
        SetShaderValue(s, l.sunDir, &dir, SHADER_UNIFORM_VEC3);
    if (l.sunColor >= 0)
        SetShaderValue(s, l.sunColor, &col, SHADER_UNIFORM_VEC3);
    if (l.ambient >= 0)
        SetShaderValue(s, l.ambient, &amb, SHADER_UNIFORM_VEC3);
    if (l.fogColor >= 0)
        SetShaderValue(s, l.fogColor, &fog, SHADER_UNIFORM_VEC3);
    if (l.fogDensity >= 0)
        SetShaderValue(s, l.fogDensity, &fogD, SHADER_UNIFORM_FLOAT);
}
} // namespace

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

    // Défauts équivalents aux anciennes constantes : jamais de rendu noir si
    // setSceneLighting n'est pas appelé.
    _impl->lightScene = cacheSceneLocs(s);
    applySceneUniforms(s, _impl->lightScene, Vector3{-0.30f, -0.86f, -0.39f}, Vector3{1.00f, 0.96f, 0.88f},
                       Vector3{0.42f, 0.44f, 0.52f}, Vector3{0, 0, 0}, 0.0f);

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
    _impl->instScene = cacheSceneLocs(s);
    applySceneUniforms(s, _impl->instScene, Vector3{-0.30f, -0.86f, -0.39f}, Vector3{1.00f, 0.96f, 0.88f},
                       Vector3{0.42f, 0.44f, 0.52f}, Vector3{0, 0, 0}, 0.0f);
    TraceLog(LOG_INFO, "loadInstancingShader: '%s' ready", vs.c_str());
    return true;
}

bool RaylibEngine::loadFloorShader(const std::string &vs, const std::string &fs)
{
    if (!FileExists(vs.c_str()) || !FileExists(fs.c_str()))
    {
        TraceLog(LOG_WARNING, "loadFloorShader: missing shader files — flat floor kept");
        return false;
    }
    Shader s = LoadShader(vs.c_str(), fs.c_str());
    if (s.id == 0)
    {
        TraceLog(LOG_WARNING, "loadFloorShader: compile failed — flat floor kept");
        return false;
    }
    _impl->floorShader = s;
    _impl->floorLoaded = true;
    _impl->floorViewPosLoc = GetShaderLocation(s, "viewPos");
    _impl->floorScene = cacheSceneLocs(s);
    _impl->floorOverlayLoc = GetShaderLocation(s, "groundOverlay");
    _impl->floorMixLoc = GetShaderLocation(s, "groundMix");
    _impl->floorCoverLoc = GetShaderLocation(s, "coverMap");
    _impl->floorCoverOnLoc = GetShaderLocation(s, "coverOn");
    _impl->floorWorldSizeLoc = GetShaderLocation(s, "worldSize");
    _impl->floorTileSizeLoc = GetShaderLocation(s, "tileSize");
    _impl->floorTimeLoc = GetShaderLocation(s, "time");
    applySceneUniforms(s, _impl->floorScene, Vector3{-0.30f, -0.86f, -0.39f}, Vector3{1.00f, 0.96f, 0.88f},
                       Vector3{0.42f, 0.44f, 0.52f}, Vector3{0, 0, 0}, 0.0f);
    setGroundSeason({1, 1, 1}, 0.0f);
    TraceLog(LOG_INFO, "loadFloorShader: '%s' ready", fs.c_str());
    return true;
}

void RaylibEngine::setSceneLighting(gfx::Vec3 lightDir, gfx::Vec3 lightColor, gfx::Vec3 ambient, gfx::Vec3 fogColor,
                                    float fogDensity)
{
    const Vector3 d = toRl(lightDir), c = toRl(lightColor), a = toRl(ambient), f = toRl(fogColor);
    if (_impl->lightLoaded)
        applySceneUniforms(_impl->lightShader, _impl->lightScene, d, c, a, f, fogDensity);
    if (_impl->instLoaded)
        applySceneUniforms(_impl->instShader, _impl->instScene, d, c, a, f, fogDensity);
    if (_impl->floorLoaded)
        applySceneUniforms(_impl->floorShader, _impl->floorScene, d, c, a, f, fogDensity);
}

void RaylibEngine::setGroundSeason(gfx::Vec3 overlay, float mix)
{
    if (!_impl->floorLoaded)
        return;
    const Vector3 o = toRl(overlay);
    if (_impl->floorOverlayLoc >= 0)
        SetShaderValue(_impl->floorShader, _impl->floorOverlayLoc, &o, SHADER_UNIFORM_VEC3);
    if (_impl->floorMixLoc >= 0)
        SetShaderValue(_impl->floorShader, _impl->floorMixLoc, &mix, SHADER_UNIFORM_FLOAT);
}

void RaylibEngine::setGroundCover(gfx::TextureHandle tex, float worldW, float worldH, float tileSize, float time,
                                  bool on)
{
    if (!_impl->floorLoaded)
        return;
    Shader s = _impl->floorShader;
    const int enabled = (on && _impl->valid(tex, _impl->textures.size()) && _impl->textures[tex].id != 0) ? 1 : 0;
    if (_impl->floorCoverOnLoc >= 0)
        SetShaderValue(s, _impl->floorCoverOnLoc, &enabled, SHADER_UNIFORM_INT);
    const Vector2 ws{worldW, worldH};
    if (_impl->floorWorldSizeLoc >= 0)
        SetShaderValue(s, _impl->floorWorldSizeLoc, &ws, SHADER_UNIFORM_VEC2);
    if (_impl->floorTileSizeLoc >= 0)
        SetShaderValue(s, _impl->floorTileSizeLoc, &tileSize, SHADER_UNIFORM_FLOAT);
    if (_impl->floorTimeLoc >= 0)
        SetShaderValue(s, _impl->floorTimeLoc, &time, SHADER_UNIFORM_FLOAT);
    if (enabled && _impl->floorCoverLoc >= 0)
    {
        // Même mécanisme que la shadow map : slot fixe hors de portée des
        // material maps raylib (0..9) ; 10 = shadow map, 11 = cover map.
        const int slot = 11;
        rlEnableShader(s.id);
        rlActiveTextureSlot(slot);
        rlEnableTexture(_impl->textures[tex].id);
        rlSetUniform(_impl->floorCoverLoc, &slot, SHADER_UNIFORM_INT, 1);
        rlActiveTextureSlot(0);
    }
}

void RaylibEngine::beginFloorShading()
{
    if (_impl->floorLoaded)
        BeginShaderMode(_impl->floorShader);
}

void RaylibEngine::endFloorShading()
{
    if (_impl->floorLoaded)
        EndShaderMode();
}

namespace
{
// FBO sans couleur : seulement une texture de profondeur échantillonnable.
RenderTexture2D loadShadowmapRT(int width, int height)
{
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    if (target.id == 0)
        return target;
    target.texture.width = width;
    target.texture.height = height;
    rlEnableFramebuffer(target.id);
    target.depth.id = rlLoadTextureDepth(width, height, false); // texture, pas renderbuffer
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19;
    target.depth.mipmaps = 1;
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    if (!rlFramebufferComplete(target.id))
    {
        rlUnloadFramebuffer(target.id);
        target.id = 0;
    }
    rlDisableFramebuffer();
    return target;
}

ShadowLocs cacheShadowLocs(Shader s)
{
    ShadowLocs l;
    l.map = GetShaderLocation(s, "shadowMap");
    l.vp = GetShaderLocation(s, "lightVP");
    l.on = GetShaderLocation(s, "shadowsOn");
    l.res = GetShaderLocation(s, "shadowMapRes");
    return l;
}
} // namespace

bool RaylibEngine::enableShadows(int resolution)
{
    _impl->shadowRT = loadShadowmapRT(resolution, resolution);
    if (_impl->shadowRT.id == 0)
    {
        TraceLog(LOG_WARNING, "enableShadows: depth FBO failed — no shadows");
        return false;
    }
    _impl->shadowRes = resolution;
    _impl->shadowsLoaded = true;
    if (_impl->lightLoaded)
        _impl->lightShadow = cacheShadowLocs(_impl->lightShader);
    if (_impl->instLoaded)
        _impl->instShadow = cacheShadowLocs(_impl->instShader);
    if (_impl->floorLoaded)
        _impl->floorShadow = cacheShadowLocs(_impl->floorShader);
    TraceLog(LOG_INFO, "enableShadows: %dx%d depth map ready", resolution, resolution);
    return true;
}

bool RaylibEngine::shadowsReady() const
{
    return _impl->shadowsLoaded;
}

void RaylibEngine::beginShadowPass(gfx::Vec3 lightDir, gfx::Vec3 sceneCentre, float sceneRadius)
{
    if (!_impl->shadowsLoaded)
        return;
    Camera3D lightCam{};
    const gfx::Vec3 d = gfx::normalize(lightDir);
    lightCam.position = toRl(gfx::sub(sceneCentre, gfx::scale(d, sceneRadius * 1.8f)));
    lightCam.target = toRl(sceneCentre);
    lightCam.up = std::fabs(d.y) > 0.95f ? Vector3{1, 0, 0} : Vector3{0, 1, 0};
    lightCam.fovy = sceneRadius * 2.0f; // largeur du volume ortho
    lightCam.projection = CAMERA_ORTHOGRAPHIC;

    BeginTextureMode(_impl->shadowRT); // fixe viewport + framebuffer taille shadowRes
    ClearBackground(WHITE);            // depth clear (la couleur n'existe pas)
    BeginMode3D(lightCam);
    _impl->lightVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

void RaylibEngine::endShadowPass()
{
    if (!_impl->shadowsLoaded)
        return;
    EndMode3D();
    EndTextureMode();

    // Publier la depth map (slot 10) + lightVP sur les trois shaders scène.
    const int slot = 10;
    const int on = 1;
    auto push = [&](Shader s, const ShadowLocs &l) {
        if (l.vp >= 0)
            SetShaderValueMatrix(s, l.vp, _impl->lightVP);
        if (l.on >= 0)
            SetShaderValue(s, l.on, &on, SHADER_UNIFORM_INT);
        if (l.res >= 0)
            SetShaderValue(s, l.res, &_impl->shadowRes, SHADER_UNIFORM_INT);
        if (l.map >= 0)
        {
            rlEnableShader(s.id);
            rlActiveTextureSlot(slot);
            rlEnableTexture(_impl->shadowRT.depth.id);
            rlSetUniform(l.map, &slot, SHADER_UNIFORM_INT, 1);
            rlActiveTextureSlot(0);
        }
    };
    if (_impl->lightLoaded)
        push(_impl->lightShader, _impl->lightShadow);
    if (_impl->instLoaded)
        push(_impl->instShader, _impl->instShadow);
    if (_impl->floorLoaded)
        push(_impl->floorShader, _impl->floorShadow);
}

namespace
{
// LoadRenderTexture force RGBA8 ; pour un pipeline HDR (valeurs > 1 pour le
// bloom/ACES) on assemble un FBO avec une texture couleur RGBA16F à la main.
RenderTexture2D loadRenderTexture16F(int width, int height)
{
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    if (target.id == 0)
        return target;
    rlEnableFramebuffer(target.id);

    target.texture.id = rlLoadTexture(nullptr, width, height, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    target.texture.width = width;
    target.texture.height = height;
    target.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    target.texture.mipmaps = 1;

    target.depth.id = rlLoadTextureDepth(width, height, true);
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19; // DEPTH_COMPONENT_24BIT (valeur raylib interne)
    target.depth.mipmaps = 1;

    rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    if (!rlFramebufferComplete(target.id))
    {
        rlUnloadFramebuffer(target.id);
        target.id = 0;
    }
    rlDisableFramebuffer();
    return target;
}

// RT HDR si le GPU le permet, sinon LDR classique (post FX conservés en 8 bits).
RenderTexture2D loadPostRT(int width, int height)
{
    RenderTexture2D rt = loadRenderTexture16F(width, height);
    if (rt.id == 0)
    {
        TraceLog(LOG_WARNING, "enablePostFx: 16F unsupported — LDR post FX");
        rt = LoadRenderTexture(width, height);
    }
    return rt;
}
} // namespace

bool RaylibEngine::enablePostFx(const std::string &extractFs, const std::string &compositeFs)
{
    if (!FileExists(extractFs.c_str()) || !FileExists(compositeFs.c_str()))
    {
        TraceLog(LOG_WARNING, "enablePostFx: missing shader files — no post FX");
        return false;
    }
    Shader ext = LoadShader(nullptr, extractFs.c_str()); // default fullscreen vs
    if (ext.id == 0)
    {
        TraceLog(LOG_WARNING, "enablePostFx: extract shader failed to compile — no post FX");
        return false;
    }
    Shader comb = LoadShader(nullptr, compositeFs.c_str());
    if (comb.id == 0)
    {
        TraceLog(LOG_WARNING, "enablePostFx: composite shader failed to compile — no post FX");
        UnloadShader(ext);
        return false;
    }
    _impl->sceneRT = loadPostRT(GetScreenWidth(), GetScreenHeight());
    _impl->bloomRT = loadPostRT(GetScreenWidth() / 2, GetScreenHeight() / 2);
    if (_impl->sceneRT.texture.id == 0 || _impl->bloomRT.texture.id == 0)
    {
        TraceLog(LOG_WARNING, "enablePostFx: render texture failed — no post FX");
        UnloadShader(ext);
        UnloadShader(comb);
        return false;
    }
    // Bilinear so the half-res glow upscales smoothly instead of blocky.
    SetTextureFilter(_impl->bloomRT.texture, TEXTURE_FILTER_BILINEAR);
    _impl->bloomExtract = ext;
    _impl->bloomCombine = comb;
    _impl->bloomResLoc = GetShaderLocation(ext, "resolution");
    _impl->postLocs.glowTex = GetShaderLocation(comb, "glowTex");
    _impl->postLocs.sunScreen = GetShaderLocation(comb, "sunScreen");
    _impl->postLocs.godray = GetShaderLocation(comb, "godray");
    _impl->postLocs.lift = GetShaderLocation(comb, "gradeLift");
    _impl->postLocs.gain = GetShaderLocation(comb, "gradeGain");
    _impl->postLocs.heat = GetShaderLocation(comb, "heat");
    _impl->postLocs.time = GetShaderLocation(comb, "time");
    _impl->bloomLoaded = true;
    TraceLog(LOG_INFO, "enablePostFx: '%s' + '%s' ready (half-res glow)", extractFs.c_str(), compositeFs.c_str());
    return true;
}

void RaylibEngine::setPostFxParams(gfx::Vec2 sunScreen01, float godrayStrength, gfx::Vec3 gradeLift,
                                   gfx::Vec3 gradeGain, float heat, float time)
{
    _impl->post.sunScreen = Vector2{sunScreen01.x, sunScreen01.y};
    _impl->post.godray = godrayStrength;
    _impl->post.lift = toRl(gradeLift);
    _impl->post.gain = toRl(gradeGain);
    _impl->post.heat = heat;
    _impl->post.time = time;
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
            _impl->sceneRT = loadPostRT(GetScreenWidth(), GetScreenHeight());
            _impl->bloomRT = loadPostRT(GetScreenWidth() / 2, GetScreenHeight() / 2);
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
    if (_impl->floorLoaded && _impl->floorViewPosLoc >= 0)
        SetShaderValue(_impl->floorShader, _impl->floorViewPosLoc, &eye, SHADER_UNIFORM_VEC3);
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

        // Pass 2: composite the scene with the upscaled glow + per-frame params.
        const auto &pl = _impl->postLocs;
        const auto &pp = _impl->post;
        BeginShaderMode(_impl->bloomCombine);
        if (pl.glowTex >= 0)
            SetShaderValueTexture(_impl->bloomCombine, pl.glowTex, _impl->bloomRT.texture);
        if (pl.sunScreen >= 0)
            SetShaderValue(_impl->bloomCombine, pl.sunScreen, &pp.sunScreen, SHADER_UNIFORM_VEC2);
        if (pl.godray >= 0)
            SetShaderValue(_impl->bloomCombine, pl.godray, &pp.godray, SHADER_UNIFORM_FLOAT);
        if (pl.lift >= 0)
            SetShaderValue(_impl->bloomCombine, pl.lift, &pp.lift, SHADER_UNIFORM_VEC3);
        if (pl.gain >= 0)
            SetShaderValue(_impl->bloomCombine, pl.gain, &pp.gain, SHADER_UNIFORM_VEC3);
        if (pl.heat >= 0)
            SetShaderValue(_impl->bloomCombine, pl.heat, &pp.heat, SHADER_UNIFORM_FLOAT);
        if (pl.time >= 0)
            SetShaderValue(_impl->bloomCombine, pl.time, &pp.time, SHADER_UNIFORM_FLOAT);
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

void RaylibEngine::drawModelPreview(gfx::ModelHandle h, int x, int y, int w, int hgt, gfx::Vec3 scale, gfx::Color tint,
                                    float yawDeg)
{
    if (w <= 0 || hgt <= 0 || !_impl->valid(h, _impl->models.size()) || _impl->models[h].meshCount == 0)
        return;

    if (_impl->previewRT.id == 0 || _impl->previewW != w || _impl->previewH != hgt)
    {
        if (_impl->previewRT.id != 0)
            UnloadRenderTexture(_impl->previewRT);
        _impl->previewRT = LoadRenderTexture(w, hgt);
        _impl->previewW = w;
        _impl->previewH = hgt;
    }

    BeginTextureMode(_impl->previewRT);
    ClearBackground(Color{0, 0, 0, 0});
    Camera3D cam{};
    cam.position = Vector3{0.0f, 24.0f, 128.0f};
    cam.target = Vector3{0.0f, 22.0f, 0.0f};
    cam.up = Vector3{0.0f, 1.0f, 0.0f};
    cam.fovy = 30.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    BeginMode3D(cam);
    DrawModelEx(_impl->models[h], Vector3{0.0f, 0.0f, 0.0f}, Vector3{0.0f, 1.0f, 0.0f}, yawDeg, toRl(scale),
                toRl(tint));
    EndMode3D();
    EndTextureMode();

    DrawTextureRec(_impl->previewRT.texture, Rectangle{0.0f, 0.0f, static_cast<float>(w), -static_cast<float>(hgt)},
                   Vector2{static_cast<float>(x), static_cast<float>(y)}, WHITE);
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

void RaylibEngine::setAdditiveBlend(bool on)
{
    rlDrawRenderBatchActive(); // flush du batch courant avant le changement d'état
    rlSetBlendMode(on ? RL_BLEND_ADDITIVE : RL_BLEND_ALPHA);
}

gfx::TextureHandle RaylibEngine::createRadialTexture(int size)
{
    Image img = GenImageGradientRadial(size, size, 0.0f, WHITE, BLANK);
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    if (t.id == 0)
        return gfx::NoHandle;
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    _impl->textures.push_back(t);
    return static_cast<gfx::TextureHandle>(_impl->textures.size() - 1);
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
    // Rasterize the default glyph set plus the French glyphs the UI needs
    // (accents, œ, guillemets, middle dot) at a size above every UI size we
    // draw (14-34px), so text only ever downscales; bilinear keeps that smooth.
    // Without an explicit codepoint list LoadFontEx covers only ASCII 32-126
    // and accents would render as "tofu".
    std::vector<int> codepoints;
    for (int c = 32; c <= 126; ++c) // ASCII printable
        codepoints.push_back(c);
    for (int c = 0xC0; c <= 0xFF; ++c) // Latin-1 accented letters À..ÿ
        codepoints.push_back(c);
    codepoints.push_back(0xAB); // « guillemet ouvrant
    codepoints.push_back(0xBB); // » guillemet fermant
    codepoints.push_back(0xB7); // · point médian
    codepoints.push_back(0x152); // Œ
    codepoints.push_back(0x153); // œ
    codepoints.push_back(0x2013); // – tiret demi-cadratin
    codepoints.push_back(0x2014); // — tiret cadratin
    codepoints.push_back(0x2026); // … points de suspension
    Font f = LoadFontEx(path.c_str(), baseSize, codepoints.data(), static_cast<int>(codepoints.size()));
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
    _impl->skyProcedural = false;
}

bool RaylibEngine::loadProceduralSky(const std::string &vs, const std::string &fs)
{
    if (!FileExists(vs.c_str()) || !FileExists(fs.c_str()))
    {
        TraceLog(LOG_WARNING, "loadProceduralSky: missing shader files");
        return false;
    }
    Shader s = LoadShader(vs.c_str(), fs.c_str());
    if (s.id == 0)
    {
        TraceLog(LOG_WARNING, "loadProceduralSky: compile failed");
        return false;
    }
    unloadSkybox(); // remplace un éventuel skybox texturé déjà chargé
    _impl->skyShader = s;
    _impl->skybox = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    _impl->skybox.materials[0].shader = s;
    _impl->skyLocs.time = GetShaderLocation(s, "time");
    _impl->skyLocs.toSun = GetShaderLocation(s, "toSun");
    _impl->skyLocs.toMoon = GetShaderLocation(s, "toMoon");
    _impl->skyLocs.sunGlow = GetShaderLocation(s, "sunGlowColor");
    _impl->skyLocs.horizon = GetShaderLocation(s, "horizonColor");
    _impl->skyLocs.zenith = GetShaderLocation(s, "zenithColor");
    _impl->skyLocs.dayTint = GetShaderLocation(s, "skyDayColor");
    _impl->skyLocs.nebula = GetShaderLocation(s, "nebulaTint");
    _impl->skyLocs.stars = GetShaderLocation(s, "starIntensity");
    _impl->skyLocs.aurora = GetShaderLocation(s, "auroraIntensity");
    _impl->skyLocs.lightning = GetShaderLocation(s, "lightning");
    _impl->skyboxLoaded = true;
    _impl->skyProcedural = true;
    TraceLog(LOG_INFO, "loadProceduralSky: '%s' ready", fs.c_str());
    return true;
}

void RaylibEngine::setSkyParams(float time, gfx::Vec3 toSun, gfx::Vec3 toMoon, gfx::Vec3 sunGlowColor,
                                gfx::Vec3 horizon, gfx::Vec3 zenith, gfx::Vec3 dayTint, gfx::Vec3 nebula, float stars,
                                float aurora, float lightning)
{
    if (!_impl->skyProcedural)
        return;
    const Shader &s = _impl->skyShader;
    const Vector3 ts = toRl(toSun), tm = toRl(toMoon), sg = toRl(sunGlowColor), ho = toRl(horizon), ze = toRl(zenith),
                  dt = toRl(dayTint), ne = toRl(nebula);
    const auto &l = _impl->skyLocs;
    if (l.time >= 0)
        SetShaderValue(s, l.time, &time, SHADER_UNIFORM_FLOAT);
    if (l.toSun >= 0)
        SetShaderValue(s, l.toSun, &ts, SHADER_UNIFORM_VEC3);
    if (l.toMoon >= 0)
        SetShaderValue(s, l.toMoon, &tm, SHADER_UNIFORM_VEC3);
    if (l.sunGlow >= 0)
        SetShaderValue(s, l.sunGlow, &sg, SHADER_UNIFORM_VEC3);
    if (l.horizon >= 0)
        SetShaderValue(s, l.horizon, &ho, SHADER_UNIFORM_VEC3);
    if (l.zenith >= 0)
        SetShaderValue(s, l.zenith, &ze, SHADER_UNIFORM_VEC3);
    if (l.dayTint >= 0)
        SetShaderValue(s, l.dayTint, &dt, SHADER_UNIFORM_VEC3);
    if (l.nebula >= 0)
        SetShaderValue(s, l.nebula, &ne, SHADER_UNIFORM_VEC3);
    if (l.stars >= 0)
        SetShaderValue(s, l.stars, &stars, SHADER_UNIFORM_FLOAT);
    if (l.aurora >= 0)
        SetShaderValue(s, l.aurora, &aurora, SHADER_UNIFORM_FLOAT);
    if (l.lightning >= 0)
        SetShaderValue(s, l.lightning, &lightning, SHADER_UNIFORM_FLOAT);
}

bool RaylibEngine::loadCelestialShader(const std::string &fs)
{
    if (!FileExists(fs.c_str()))
    {
        TraceLog(LOG_WARNING, "loadCelestialShader: missing '%s'", fs.c_str());
        return false;
    }
    Shader s = LoadShader(nullptr, fs.c_str());
    if (s.id == 0)
    {
        TraceLog(LOG_WARNING, "loadCelestialShader: compile failed");
        return false;
    }
    _impl->celestialShader = s;
    _impl->celEmissiveLoc = GetShaderLocation(s, "emissive");
    _impl->celAlphaLoc = GetShaderLocation(s, "bodyAlpha");
    _impl->celMoonLoc = GetShaderLocation(s, "isMoon");
    _impl->celTimeLoc = GetShaderLocation(s, "time");
    _impl->celestialLoaded = true;
    TraceLog(LOG_INFO, "loadCelestialShader: ready");
    return true;
}

void RaylibEngine::drawCelestial(gfx::TextureHandle tex, const gfx::Camera &cam, gfx::Vec3 toBody,
                                 float angularSizeDeg, gfx::Vec3 emissive, float alpha, bool isMoon)
{
    if (!_impl->celestialLoaded || !_impl->valid(tex, _impl->textures.size()) || alpha <= 0.01f)
        return;

    // Quad à distance fixe (dans le far plane), taille depuis l'angle apparent.
    const float dist = 800.0f;
    const float half = dist * std::tan(angularSizeDeg * 0.5f * DEG2RAD);
    const gfx::Vec3 dir = gfx::normalize(toBody);
    const gfx::Vec3 centre = gfx::add(cam.position, gfx::scale(dir, dist));
    gfx::Vec3 right = gfx::normalize({-dir.z, 0.0f, dir.x}); // horizontal, perpendiculaire à dir
    if (gfx::length(right) < 0.5f)
        right = {1, 0, 0};
    // up = right x dir (et non dir x right, qui pointe vers le BAS et
    // affichait Luan/Palasse tête en bas).
    const gfx::Vec3 up = gfx::normalize(gfx::Vec3{right.y * dir.z - right.z * dir.y, right.z * dir.x - right.x * dir.z,
                                                  right.x * dir.y - right.y * dir.x});
    const gfx::Vec3 a = gfx::add(gfx::add(centre, gfx::scale(right, -half)), gfx::scale(up, half));
    const gfx::Vec3 b = gfx::add(gfx::add(centre, gfx::scale(right, -half)), gfx::scale(up, -half));
    const gfx::Vec3 c = gfx::add(gfx::add(centre, gfx::scale(right, half)), gfx::scale(up, -half));
    const gfx::Vec3 d = gfx::add(gfx::add(centre, gfx::scale(right, half)), gfx::scale(up, half));

    const Vector3 em = toRl(emissive);
    const int moon = isMoon ? 1 : 0;
    const float now = static_cast<float>(GetTime());
    if (_impl->celEmissiveLoc >= 0)
        SetShaderValue(_impl->celestialShader, _impl->celEmissiveLoc, &em, SHADER_UNIFORM_VEC3);
    if (_impl->celAlphaLoc >= 0)
        SetShaderValue(_impl->celestialShader, _impl->celAlphaLoc, &alpha, SHADER_UNIFORM_FLOAT);
    if (_impl->celMoonLoc >= 0)
        SetShaderValue(_impl->celestialShader, _impl->celMoonLoc, &moon, SHADER_UNIFORM_INT);
    if (_impl->celTimeLoc >= 0)
        SetShaderValue(_impl->celestialShader, _impl->celTimeLoc, &now, SHADER_UNIFORM_FLOAT);

    // Derrière tout : depth off, comme le skybox.
    rlDisableDepthMask();
    rlDisableDepthTest();
    BeginShaderMode(_impl->celestialShader);
    Texture2D &t = _impl->textures[tex];
    rlSetTexture(t.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlNormal3f(-dir.x, -dir.y, -dir.z);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(a.x, a.y, a.z);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(b.x, b.y, b.z);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(c.x, c.y, c.z);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(d.x, d.y, d.z);
    rlEnd();
    rlSetTexture(0);
    EndShaderMode();
    rlEnableDepthTest();
    rlEnableDepthMask();
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
    beginFloorShading();
    if (_impl->valid(dark, _impl->textures.size()))
        drawFloorBatch(_impl->textures[dark], true, w, h, tileSize, quadSize, surfaceY);
    if (_impl->valid(light, _impl->textures.size()))
        drawFloorBatch(_impl->textures[light], false, w, h, tileSize, quadSize, surfaceY);
    endFloorShading();
}
