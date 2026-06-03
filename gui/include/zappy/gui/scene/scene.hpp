#pragma once

#include <cstdint>
#include <vector>

namespace zappy::gui::scene
{

// Renderer-facing snapshot of the world, fed by net::GuiClient and drawn by the
// renderer. Kept deliberately POD-ish so it's cheap to update per event. P4 owns it.
struct TileView
{
    int x{};
    int y{};
    int resources[7]{};
};

struct PlayerView
{
    std::uint32_t id{};
    int x{};
    int y{};
    int orientation{1};
    int level{1};
    std::uint16_t team{};
};

struct Scene
{
    int width{};
    int height{};
    std::vector<TileView> tiles;
    std::vector<PlayerView> players;
    // TODO(P4): eggs, broadcasts, incantations, interpolation between ticks.
};

} // namespace zappy::gui::scene
