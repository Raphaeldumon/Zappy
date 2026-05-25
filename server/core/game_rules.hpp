#pragma once

#include "core/types.hpp"

#include <array>

namespace zappy::core {

// Static game rules from the subject. Constants live here so server, simulator and
// tests share one source of truth. P1 owns elevation/vision/broadcast logic.

inline constexpr int MAX_LEVEL = 8;

// A drone starts with this many life units worth of food; each food = 126 time units.
inline constexpr int LIFE_UNITS_PER_FOOD = 126;
inline constexpr int STARTING_FOOD = 10;

// Elevation requirements per target level (index 0 => ritual to reach level 2).
// {players_required, linemate, deraumere, sibur, mendiane, phiras, thystame}
struct ElevationRequirement {
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

// TODO(P1): can_elevate(world, x, y, level), look(player) vision cone, broadcast
//           direction (0..8) with toroidal shortest path. See docs 02_server.md.

} // namespace zappy::core
