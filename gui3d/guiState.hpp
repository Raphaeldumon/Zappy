#pragma once

#include "aiPlayer.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Render-facing snapshot of the game, kept alongside GameMap. GameMap owns the
// tiles (resources + a mirror of which player ids stand on each tile, for the
// existing renderer); this struct owns the authoritative per-entity data the
// tiles can't express: the players (each an aiPlayer with cell/orientation/
// level/team) and the live egg list. The parser mutates both in lockstep.
struct EggInfo {
    int x{0};
    int y{0};
};

struct GuiState {
    std::unordered_map<std::uint32_t, aiPlayer> players;
    std::unordered_map<std::uint32_t, EggInfo>  eggs;
    std::vector<std::string>                    teams;
    std::unordered_set<long long>               incanting; // tile keys (y*W+x) mid-incantation
    int                                         frequency{0};
    bool                                        hasWinner{false};
    std::string                                 winner;
};
