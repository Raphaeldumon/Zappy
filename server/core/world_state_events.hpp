#pragma once

#include "core/types.hpp"

#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace zappy::core
{

// Event structs emitted by WorldState game-logic methods.
// Server.cpp converts these to GUI wire strings and broadcasts them.
// Keeping them here lets WorldState stay network-free.

struct EvPlayerNew
{
    PlayerId id;
    int x, y;
    Orientation o;
    int level;
    TeamId team;
};
struct EvPlayerMove
{
    PlayerId id;
    int x, y;
    Orientation o;
};
struct EvPlayerLevel
{
    PlayerId id;
    int level;
};
struct EvPlayerInv
{
    PlayerId id;
    int x, y;
    ResourceSet inv;
};
struct EvPlayerExpel
{
    PlayerId id;
};
struct EvPlayerBroadcast
{
    PlayerId id;
    std::string text;
};
struct EvIncantStart
{
    int x, y, level;
    std::vector<PlayerId> participants;
};
struct EvIncantEnd
{
    int x, y;
    bool success;
};
struct EvPlayerFork
{
    PlayerId id;
};
struct EvResourceDrop
{
    PlayerId id;
    int resource_index;
};
struct EvResourceTake
{
    PlayerId id;
    int resource_index;
};
struct EvPlayerDie
{
    PlayerId id;
};
struct EvEggNew
{
    EggId egg;
    PlayerId layer;
    int x, y;
};
struct EvEggHatch
{
    EggId egg;
};
struct EvEggDie
{
    EggId egg;
};
struct EvTileUpdate
{
    int x, y;
    ResourceSet resources;
};
struct EvGameEnd
{
    std::string team_name;
};

using WorldEvent = std::variant<EvPlayerNew, EvPlayerMove, EvPlayerLevel, EvPlayerInv, EvPlayerExpel, EvPlayerBroadcast,
                                EvIncantStart, EvIncantEnd, EvPlayerFork, EvResourceDrop, EvResourceTake, EvPlayerDie,
                                EvEggNew, EvEggHatch, EvEggDie, EvTileUpdate, EvGameEnd>;

using EventCallback = std::function<void(WorldEvent)>;

} // namespace zappy::core
