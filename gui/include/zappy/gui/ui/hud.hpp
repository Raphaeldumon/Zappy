#pragma once

namespace zappy::gui::ui {

// Dear ImGui HUD: top status bar, team panel, timeline. P4 owns this; it lights up
// once ImGui is wired via vcpkg (see gui/CMakeLists.txt gui feature).
class Hud {
public:
    void draw();   // TODO(P4): ImGui windows
    void toggle(); // F3
private:
    bool visible_{true};
};

} // namespace zappy::gui::ui
