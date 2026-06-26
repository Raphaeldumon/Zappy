#pragma once
#include "gfxTypes.hpp"

#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// RaylibEngine — the graphics facade. This is the ONLY place raylib is used:
// the header exposes nothing but neutral gfx:: vocabulary types and opaque
// handles, and the raylib headers are pulled in solely by raylibWrapper.cpp
// (behind a PIMPL, so even the raylib value types stay out of this header).
// Everything the GUI needs — window, input, audio, models, textures, the
// skybox, the batched floor, 2D/3D drawing — is a method here, which is what
// keeps interface.{hpp,cpp} completely raylib-free and the renderer swappable.
//
// Owns the GL/audio context and every GPU/audio resource it hands out as a
// handle; construction opens the window, destruction frees everything.
// Non-copyable and non-movable (unique GL context).
// ---------------------------------------------------------------------------

namespace gfx {
    // raylib-backed logging (kept for identical TraceLog formatting); defined
    // in raylibWrapper.cpp so callers need no raylib include.
    void logWarn(const char* fmt, ...);
    void logInfo(const char* fmt, ...);
}

class RaylibEngine {
    public:
        RaylibEngine(int width, int height, const std::string& title);
        ~RaylibEngine();

        RaylibEngine(const RaylibEngine&)            = delete;
        RaylibEngine& operator=(const RaylibEngine&) = delete;
        RaylibEngine(RaylibEngine&&)                 = delete;
        RaylibEngine& operator=(RaylibEngine&&)      = delete;

        // --- Window / frame lifecycle ---
        bool shouldClose() const;
        void beginDrawing();   // BeginDrawing + clear to black
        void endDrawing();
        void disableCursor();  // capture mouse for free-look
        void enableCursor();
        int  screenWidth() const;
        int  screenHeight() const;
        void drawFps(int x, int y);

        // --- Input ---
        bool      keyDown(gfx::Key key) const;
        bool      keyPressed(gfx::Key key) const;
        int       charPressed() const;       // 0 when the queue is empty
        bool      mousePressed(gfx::MouseBtn btn) const;
        gfx::Vec2 mouseDelta() const;
        float     mouseWheel() const;
        double    time() const;
        float     frameTime() const;

        // --- Audio ---
        bool             initAudio();   // true if the device is ready
        void             closeAudio();
        gfx::MusicHandle loadMusic(const std::string& path); // looping; NoHandle on fail
        void             playMusic(gfx::MusicHandle h);
        void             stopMusic(gfx::MusicHandle h);
        void             pauseMusic(gfx::MusicHandle h);
        void             resumeMusic(gfx::MusicHandle h);
        void             updateMusic(gfx::MusicHandle h);
        void             setMusicVolume(gfx::MusicHandle h, float volume);
        void             unloadMusic(gfx::MusicHandle h);

        // --- Textures ---
        gfx::TextureHandle loadTexture(const std::string& path); // NoHandle on fail
        void               setTextureBilinear(gfx::TextureHandle h);
        void               unloadTexture(gfx::TextureHandle h);

        // --- Models ---
        // loadModel returns NoHandle if the file is missing or has no mesh.
        gfx::ModelHandle loadModel(const std::string& path);
        gfx::BBox        modelBounds(gfx::ModelHandle h) const;
        void             translateModel(gfx::ModelHandle h, gfx::Vec3 t); // bake into transform
        void             bindModelTexture(gfx::ModelHandle h, gfx::TextureHandle tex); // all materials
        void             unloadModel(gfx::ModelHandle h);

        // --- Model animations ---
        gfx::AnimSetHandle loadAnimations(const std::string& path); // NoHandle if none
        int                animCount(gfx::AnimSetHandle s) const;
        std::string        animName(gfx::AnimSetHandle s, int i) const;
        int                animFrameCount(gfx::AnimSetHandle s, int i) const;
        void               applyPose(gfx::ModelHandle m, gfx::AnimSetHandle s, int i, float frame);
        void               unloadAnimations(gfx::AnimSetHandle s);

        // --- 3D drawing ---
        void beginMode3D(const gfx::Camera& cam);
        void endMode3D();
        void drawModelEx(gfx::ModelHandle h, gfx::Vec3 pos, gfx::Vec3 axis,
                         float angleDeg, gfx::Vec3 scale, gfx::Color tint);
        void drawCube(gfx::Vec3 pos, float w, float h, float l, gfx::Color c);
        void drawPlane(gfx::Vec3 center, gfx::Vec2 size, gfx::Color c);
        void drawSphere(gfx::Vec3 center, float radius, gfx::Color c);
        void drawLine3D(gfx::Vec3 a, gfx::Vec3 b, gfx::Color c);
        gfx::Vec2 worldToScreen(const gfx::Camera& cam, gfx::Vec3 world) const;
        gfx::Ray  screenToWorldRay(const gfx::Camera& cam, gfx::Vec2 screen) const;

        // --- 2D drawing ---
        void drawText(const std::string& text, int x, int y, int fontSize, gfx::Color c);
        void drawRect(int x, int y, int w, int h, gfx::Color c);
        void drawRectLines(int x, int y, int w, int h, gfx::Color c);
        void drawLine(int x1, int y1, int x2, int y2, gfx::Color c);
        void drawCircle(int cx, int cy, float radius, gfx::Color c);

        // --- Composite scene primitives (raylib/rlgl-coupled, owned here) ---
        bool loadSkybox(const std::string& png, const std::string& vs, const std::string& fs);
        void drawSkybox();   // call first inside beginMode3D
        void unloadSkybox();
        // Two batched checkerboard passes (dark cells then light cells), each a
        // single rlBegin(RL_QUADS) chunked under the rlgl buffer.
        void drawCheckerFloor(gfx::TextureHandle dark, gfx::TextureHandle light,
                              int w, int h, float tileSize, float quadSize, float surfaceY);

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};
