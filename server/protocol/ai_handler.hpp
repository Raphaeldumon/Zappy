#pragma once

#include "core/world_state.hpp"
#include "zappy/protocol/ai_protocol.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zappy::protocol
{

struct ParsedCommand
{
    zappy::protocol::ai::Command cmd;
    std::string arg;        // resource name for Take/Set; text for Broadcast; empty otherwise
    int resource_index{-1}; // pre-resolved for Take/Set, -1 otherwise
};

// Parse one command line from an AI client.
// Returns nullopt for unknown/malformed commands (caller sends "ko").
[[nodiscard]] std::optional<ParsedCommand> parse_ai_command(std::string_view line);

// Format the Look response.
// Tiles separated by ", ", items within a tile separated by space.
// "player" is emitted once per player on the tile, then resource names repeated by count.
[[nodiscard]] std::string fmt_look(const std::vector<core::WorldState::LookTile> &tiles);

// Format the Inventory response.
// "[food N, linemate N, deraumere N, sibur N, mendiane N, phiras N, thystame N]"
[[nodiscard]] std::string fmt_inventory(const core::ResourceSet &inv);

// Format Connect_nbr response: just the slot count as a string.
[[nodiscard]] std::string fmt_connect_nbr(int slots);

} // namespace zappy::protocol
