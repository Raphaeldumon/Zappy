#pragma once

// AI <-> Server protocol contract (G-YEP-400_zappy.pdf).
// SACRED: any change here needs an ADR + the 3 leads' approval (see CODEOWNERS).
//
// See docs/01_architecture/06_protocols.md for the authoritative spec.

#include <array>
#include <string_view>

namespace zappy::protocol::ai {

// Handshake lines (server side, each terminated by '\n' on the wire).
inline constexpr std::string_view WELCOME = "WELCOME";
inline constexpr std::string_view GRAPHIC_TEAM = "GRAPHIC"; // switches to GUI protocol

// Standard responses.
inline constexpr std::string_view OK = "ok";
inline constexpr std::string_view KO = "ko";
inline constexpr std::string_view DEAD = "dead";

// The 12 client commands. Underlying values are stable identifiers; do not reorder
// without an ADR.
enum class Command {
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
    Count_ // sentinel
};

// Time cost of each command, expressed in 1/f time units (the `t` in `cost/f`).
// Connect_nbr has no time cost.
inline constexpr int time_cost(Command c) noexcept {
    switch (c) {
    case Command::Inventory:
        return 1;
    case Command::Fork:
        return 42;
    case Command::Incantation:
        return 300;
    case Command::ConnectNbr:
        return 0;
    default:
        return 7; // Forward, Right, Left, Look, Broadcast, Eject, Take, Set
    }
}

// Max number of commands buffered server-side per client; extras are dropped.
inline constexpr int MAX_COMMAND_QUEUE = 10;

// The 7 resources, ordered as they appear on the wire (q0..q6).
enum class Resource {
    Food = 0,
    Linemate,
    Deraumere,
    Sibur,
    Mendiane,
    Phiras,
    Thystame,
    Count_ // 7
};

inline constexpr int RESOURCE_COUNT = static_cast<int>(Resource::Count_);

inline constexpr std::array<std::string_view, RESOURCE_COUNT> RESOURCE_NAMES = {
    "food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"};

} // namespace zappy::protocol::ai
