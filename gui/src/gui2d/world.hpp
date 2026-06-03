#pragma once

// Minimal world model for the throwaway 2D debug GUI. Not the final renderer —
// this exists so we can *see* what the server is emitting over the GUI protocol.
// Kept self-contained (no dependency on the Vulkan-side scene types).

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace zappy::gui2d
{

constexpr int RESOURCE_COUNT = 7;

// Indices match docs/01_architecture/06_protocols.md section 3.
inline const char *resource_name(int i)
{
    static const char *names[RESOURCE_COUNT] = {"food",     "linemate", "deraumere", "sibur",
                                                "mendiane", "phiras",   "thystame"};
    return (i >= 0 && i < RESOURCE_COUNT) ? names[i] : "?";
}

struct Tile
{
    int resources[RESOURCE_COUNT]{};
};

struct Player
{
    int x{};
    int y{};
    int orientation{1}; // 1=N 2=E 3=S 4=W
    int level{1};
    std::string team;
};

struct Egg
{
    int x{};
    int y{};
};

// Whole observable state. Maps keyed by the server's numeric ids.
struct World
{
    int width{0};
    int height{0};
    int time_unit{0};
    std::vector<Tile> tiles; // size width*height, row-major
    std::vector<std::string> teams;
    std::unordered_map<std::uint32_t, Player> players;
    std::unordered_map<std::uint32_t, Egg> eggs;
    std::string last_message; // smg / seg banner

    void resize(int w, int h)
    {
        width = w;
        height = h;
        tiles.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), Tile{});
    }

    Tile *tile_at(int x, int y)
    {
        if (x < 0 || y < 0 || x >= width || y >= height)
            return nullptr;
        return &tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    }
};

} // namespace zappy::gui2d
