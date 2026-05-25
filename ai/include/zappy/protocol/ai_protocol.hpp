#pragma once

#include <array>
#include <string_view>

namespace zappy::protocol::ai
{

inline constexpr std::string_view WELCOME = "WELCOME";
inline constexpr std::string_view GRAPHIC_TEAM = "GRAPHIC";
inline constexpr std::string_view OK = "ok";
inline constexpr std::string_view KO = "ko";
inline constexpr std::string_view DEAD = "dead";

enum class Command
{
    Forward = 0,
    Right,
    Left,
    Look,
    Inventory,
    Broadcast,
    ConnectNbr,
    Fork,
    Eject,
    Take,
    Set,
    Incantation,
    Count_
};

inline constexpr int time_cost(Command c) noexcept
{
    switch (c)
    {
    case Command::Inventory:
        return 1;
    case Command::Fork:
        return 42;
    case Command::Incantation:
        return 300;
    case Command::ConnectNbr:
        return 0;
    default:
        return 7;
    }
}

inline constexpr int MAX_COMMAND_QUEUE = 10;

enum class Resource
{
    Food = 0,
    Linemate,
    Deraumere,
    Sibur,
    Mendiane,
    Phiras,
    Thystame,
    Count_
};

inline constexpr int RESOURCE_COUNT = static_cast<int>(Resource::Count_);

inline constexpr std::array<std::string_view, RESOURCE_COUNT> RESOURCE_NAMES = {
    "food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"};

} // namespace zappy::protocol::ai
