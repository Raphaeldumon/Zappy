#pragma once

#include "core/types.hpp"
#include "core/world_state.hpp"

#include <array>

namespace zappy::core
{

// Static game rules from the subject. Constants live here so server, simulator and
// tests share one source of truth. P1 owns elevation/vision/broadcast logic.

inline constexpr int MAX_LEVEL = 8;

// A drone starts with this many life units worth of food; each food = 126 time units.
inline constexpr int LIFE_UNITS_PER_FOOD = 126;
inline constexpr int STARTING_FOOD = 10;

// Elevation requirements per target level (index 0 => ritual to reach level 2).
// {players_required, linemate, deraumere, sibur, mendiane, phiras, thystame}
struct ElevationRequirement
{
    int players;
    std::array<int, 6> stones; // linemate..thystame (food excluded)
};

inline constexpr std::array<ElevationRequirement, 7> ELEVATION_TABLE = {{
    {1, {1, 0, 0, 0, 0, 0}}, // 1 -> 2
    {2, {1, 1, 1, 0, 0, 0}}, // 2 -> 3
    {2, {2, 0, 1, 0, 2, 0}}, // 3 -> 4
    {4, {1, 1, 2, 0, 1, 0}}, // 4 -> 5
    {4, {1, 2, 1, 3, 0, 0}}, // 5 -> 6
    {6, {1, 2, 3, 0, 1, 0}}, // 6 -> 7
    {6, {2, 2, 2, 2, 2, 1}}, // 7 -> 8
}};

// ---------------------------------------------------------------------------
// Free functions — game rule evaluation
// ---------------------------------------------------------------------------

// Returns true if incantation at (x,y) can proceed for players at current_level.
// Checks same-level player count AND stone quantities on the tile.
[[nodiscard]] inline bool can_elevate(const WorldState &w, int x, int y, int current_level)
{
    if (current_level < 1 || current_level >= MAX_LEVEL)
        return false;
    const auto &req = ELEVATION_TABLE[static_cast<std::size_t>(current_level - 1)];

    // Count living players of the right level on this tile
    auto pids = w.players_at(x, y);
    int count = 0;
    for (auto pid : pids)
    {
        const auto *p = w.find_player(pid);
        if (p && p->alive && p->level == current_level)
            ++count;
    }
    if (count < req.players)
        return false;

    // Check stones (tile.resources[1..6] = linemate..thystame)
    const auto &tile = w.at(x, y);
    for (std::size_t s = 0; s < req.stones.size(); ++s)
        if (tile.resources[s + 1] < req.stones[s])
            return false;

    return true;
}

// Consume elevation stones from tile. Call only after can_elevate() == true.
inline void consume_elevation_stones(WorldState &w, int x, int y, int current_level)
{
    if (current_level < 1 || current_level >= MAX_LEVEL)
        return;
    const auto &req = ELEVATION_TABLE[static_cast<std::size_t>(current_level - 1)];
    auto &tile = w.at(x, y);
    for (std::size_t s = 0; s < req.stones.size(); ++s)
        tile.resources[s + 1] -= req.stones[s];
}

// Orientation helpers (also used by broadcast_direction; defined here to avoid duplication).
[[nodiscard]] inline std::pair<int, int> orient_facing(Orientation o) noexcept
{
    switch (o)
    {
    case Orientation::North:
        return {0, -1};
    case Orientation::East:
        return {1, 0};
    case Orientation::South:
        return {0, 1};
    case Orientation::West:
        return {-1, 0};
    }
    return {0, 0};
}

[[nodiscard]] inline std::pair<int, int> orient_right(Orientation o) noexcept
{
    switch (o)
    {
    case Orientation::North:
        return {1, 0};
    case Orientation::East:
        return {0, 1};
    case Orientation::South:
        return {-1, 0};
    case Orientation::West:
        return {0, -1};
    }
    return {0, 0};
}

// Compute the broadcast direction K (0..8) from sender (sx,sy) to receiver (rx,ry).
// K=0 means same tile. K=1 means straight ahead of receiver, clockwise to K=8.
// Uses integer arithmetic only (no floating-point), toroidal shortest path.
[[nodiscard]] inline int broadcast_direction(int sx, int sy, int rx, int ry, Orientation recv_orient, int W,
                                             int H) noexcept
{
    if (sx == rx && sy == ry)
        return 0;

    int dx = sx - rx;
    int dy = sy - ry;
    // Toroidal shortest path
    if (dx > W / 2)
        dx -= W;
    if (dx < -(W / 2))
        dx += W;
    if (dy > H / 2)
        dy -= H;
    if (dy < -(H / 2))
        dy += H;

    auto [fw_x, fw_y] = orient_facing(recv_orient);
    auto [rw_x, rw_y] = orient_right(recv_orient);

    int fwd = dx * fw_x + dy * fw_y;
    int rgt = dx * rw_x + dy * rw_y;

    if (fwd > 0 && rgt == 0)
        return 1;
    if (fwd > 0 && rgt > 0 && fwd >= rgt)
        return 2;
    if (fwd == 0 && rgt > 0)
        return 3;
    if (fwd < 0 && rgt > 0 && rgt >= -fwd)
        return 4;
    if (fwd < 0 && rgt == 0)
        return 5;
    if (fwd < 0 && rgt < 0 && -fwd >= -rgt)
        return 6;
    if (fwd == 0 && rgt < 0)
        return 7;
    if (fwd > 0 && rgt < 0 && fwd >= -rgt)
        return 8;
    // diagonal tie-breaks
    if (fwd > 0 && rgt > 0)
        return 2;
    if (fwd < 0 && rgt > 0)
        return 4;
    if (fwd < 0 && rgt < 0)
        return 6;
    if (fwd > 0 && rgt < 0)
        return 8;
    return 1;
}

} // namespace zappy::core
