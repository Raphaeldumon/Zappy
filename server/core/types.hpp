#pragma once

#include "zappy/protocol/ai_protocol.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace zappy::core
{

using PlayerId = std::uint32_t;
using EggId = std::uint32_t;
using TeamId = std::uint16_t;

inline constexpr int RESOURCE_COUNT = protocol::ai::RESOURCE_COUNT;

// Quantities of the 7 resources, indexed by protocol::ai::Resource (q0..q6).
using ResourceSet = std::array<int, RESOURCE_COUNT>;

enum class Orientation : std::uint8_t
{
    North = 1,
    East = 2,
    South = 3,
    West = 4
};

// A single map cell. Holds whatever resources sit on the ground.
struct Tile
{
    ResourceSet resources{};
    std::vector<std::uint64_t> food_expirations{};
};

// A team registered with the server.
struct Team
{
    TeamId id{};
    std::string name;
    int slots_available{0}; // decrements on AI connect, increments on Fork
};

// A connected player (a "drone").
struct Player
{
    PlayerId id{};
    TeamId team{};
    int x{};
    int y{};
    Orientation orientation{Orientation::North};
    int level{1};
    ResourceSet inventory{};
    int life_units{};
    bool alive{true};
    bool incanting{false};          // frozen during Incantation ritual
    std::uint64_t food_event_id{0}; // EventScheduler id, cancel on death
    std::uint64_t cmd_event_id{0};  // EventScheduler id, cancel on disconnect
};

// An egg laid by Fork; hatches into a future player slot.
struct Egg
{
    EggId id{};
    PlayerId layer{};
    TeamId team{};
    int x{};
    int y{};
    bool hatched{false};
};

} // namespace zappy::core
