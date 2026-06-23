#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Render-facing snapshot of the game, kept alongside GameMap. GameMap owns the
// tiles (resources + a mirror of which player ids stand on each tile, for the
// existing renderer); this struct owns the authoritative per-entity data the
// tiles can't express: a player's current cell/orientation/level/team and the
// live egg list. The parser mutates both in lockstep.
struct PlayerInfo {
    int         x{0};
    int         y{0};
    int         orient{1}; // wire orientation: N=1, E=2, S=3, W=4
    int         level{1};
    std::string team;
};

struct EggInfo {
    int x{0};
    int y{0};
};

struct GuiState {
    std::unordered_map<std::uint32_t, PlayerInfo> players;
    std::unordered_map<std::uint32_t, EggInfo>    eggs;
    std::vector<std::string>                      teams;
    std::unordered_set<long long>                 incanting; // tile keys (y*W+x) mid-incantation
    int                                           frequency{0};
    bool                                          hasWinner{false};
    std::string                                   winner;
};
