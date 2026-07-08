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
struct EggInfo
{
    int x{0};
    int y{0};
};

// One-shot animation triggers the parser emits when it sees an action packet the
// renderer wants to react to. The renderer drains this queue every frame, turning
// each event into a per-player one-shot clip (Kick/Pickup/Jump) or a death ghost.
// Death carries the last pose because pdi erases the player before we can draw it.
enum class PlayerAnimEventKind : std::uint8_t
{
    Kick,   // pex: this player ejected others
    Pickup, // pgt: this player collected an item
    Jump,   // pbc: this player broadcast a message
    Death   // pdi: this player died
};

struct PlayerAnimEvent
{
    std::uint32_t id{0};
    PlayerAnimEventKind kind{PlayerAnimEventKind::Kick};
    int x{0};
    int y{0};
    Orientation orientation{Orientation::North};
};

// Narrative events the parser emits for the on-screen event feed. Unlike
// animEvents these carry the human-facing payload (broadcast text, team name,
// levels) so the renderer can print them without re-deriving game logic.
enum class GameEventKind : std::uint8_t
{
    Join,        // pnw: text = team
    Broadcast,   // pbc: text = raw message (also feeds the speech bubble)
    Death,       // pdi: text = team
    LevelUp,     // plv with a higher level: value = new level
    IncantStart, // pic: x/y set, value = target level
    IncantEnd,   // pie: x/y set, value = result (1 ok, 0 failed, -1 unknown)
    Fork,        // pfk: player laid an egg
    Eject,       // pex: player ejected others
    Meteor,      // smg meteor: x/y set
    Weather,     // wth: text = weather, value = duration ticks
    Win          // seg: text = winning team
};

struct GameEvent
{
    GameEventKind kind{GameEventKind::Join};
    std::uint32_t id{0}; // player id when the event is about one (0 otherwise)
    int x{0};
    int y{0};
    int value{0};
    std::string text;
};

struct GuiState
{
    std::unordered_map<std::uint32_t, aiPlayer> players;
    std::unordered_map<std::uint32_t, EggInfo> eggs;
    std::vector<std::string> teams;
    std::unordered_set<long long> incanting; // tile keys (y*W+x) mid-incantation
    std::vector<PlayerAnimEvent> animEvents; // drained by the renderer each frame
    std::vector<GameEvent> feedEvents;       // drained by the renderer each frame
    int frequency{0};
    std::string season{"spring"};
    std::string weather{"clear"};
    int weatherDurationTicks{0};
    bool hasWinner{false};
    std::string winner;
};
