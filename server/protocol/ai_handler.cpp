#include "protocol/ai_handler.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace zappy::protocol
{

using Cmd = zappy::protocol::ai::Command;

static const std::unordered_map<std::string, Cmd> CMD_MAP = {
    {"Forward", Cmd::Forward},
    {"Right", Cmd::Right},
    {"Left", Cmd::Left},
    {"Look", Cmd::Look},
    {"Inventory", Cmd::Inventory},
    {"Broadcast", Cmd::Broadcast},
    {"Connect_nbr", Cmd::ConnectNbr},
    {"Fork", Cmd::Fork},
    {"Eject", Cmd::Eject},
    {"Take", Cmd::Take},
    {"Set", Cmd::Set},
    {"Incantation", Cmd::Incantation},
};

static int resource_index_of(std::string_view name)
{
    for (std::size_t i = 0; i < ai::RESOURCE_NAMES.size(); ++i)
        if (ai::RESOURCE_NAMES[i] == name)
            return static_cast<int>(i);
    return -1;
}

std::optional<ParsedCommand> parse_ai_command(std::string_view line)
{
    // Split on first space
    auto sp = line.find(' ');
    std::string_view verb = (sp == std::string_view::npos) ? line : line.substr(0, sp);
    std::string_view arg = (sp == std::string_view::npos) ? "" : line.substr(sp + 1);

    // Trim trailing spaces from verb
    while (!verb.empty() && verb.back() == ' ')
        verb.remove_suffix(1);

    auto it = CMD_MAP.find(std::string(verb));
    if (it == CMD_MAP.end())
        return std::nullopt;

    ParsedCommand cmd;
    cmd.cmd = it->second;
    cmd.arg = std::string(arg);

    if (cmd.cmd == Cmd::Take || cmd.cmd == Cmd::Set)
    {
        // Trim arg
        while (!cmd.arg.empty() && cmd.arg.front() == ' ')
            cmd.arg.erase(cmd.arg.begin());
        while (!cmd.arg.empty() && cmd.arg.back() == ' ')
            cmd.arg.pop_back();
        cmd.resource_index = resource_index_of(cmd.arg);
        if (cmd.resource_index < 0)
            return std::nullopt; // unknown resource
    }
    return cmd;
}

std::string fmt_look(const std::vector<core::WorldState::LookTile> &tiles)
{
    std::string result = "[";
    for (std::size_t i = 0; i < tiles.size(); ++i)
    {
        if (i > 0)
            result += ", ";
        const auto &t = tiles[i];
        std::string tile_str;
        for (int p = 0; p < t.player_count; ++p)
        {
            if (!tile_str.empty())
                tile_str += ' ';
            tile_str += "player";
        }
        for (std::size_t r = 0; r < t.resources.size(); ++r)
        {
            for (int q = 0; q < t.resources[r]; ++q)
            {
                if (!tile_str.empty())
                    tile_str += ' ';
                tile_str += ai::RESOURCE_NAMES[r];
            }
        }
        result += tile_str;
    }
    result += ']';
    return result;
}

std::string fmt_inventory(const core::ResourceSet &inv)
{
    std::string result = "[";
    for (std::size_t r = 0; r < inv.size(); ++r)
    {
        if (r > 0)
            result += ", ";
        result += std::string(ai::RESOURCE_NAMES[r]);
        result += ' ';
        result += std::to_string(inv[r]);
    }
    result += ']';
    return result;
}

std::string fmt_connect_nbr(int slots)
{
    return std::to_string(slots);
}

} // namespace zappy::protocol
