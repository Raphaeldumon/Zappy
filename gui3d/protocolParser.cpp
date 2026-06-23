#include "protocolParser.hpp"

#include "gameMap.hpp"
#include "guiState.hpp"

#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace {

// "#3" -> 3, "3" -> 3. Returns false on garbage.
bool parseId(const std::string& tok, std::uint32_t& out)
{
    const char* s = tok.c_str();
    if (*s == '#')
        ++s;
    if (*s == '\0')
        return false;
    char* end = nullptr;
    unsigned long v = std::strtoul(s, &end, 10);
    if (end == s || *end != '\0')
        return false;
    out = static_cast<std::uint32_t>(v);
    return true;
}

bool inBounds(const GameMap& map, int x, int y)
{
    return x >= 0 && y >= 0 && x < map.getWidth() && y < map.getHeight();
}

long long tileKey(const GameMap& map, int x, int y)
{
    return static_cast<long long>(y) * map.getWidth() + x;
}

// Drop a player from whatever tile the registry says it's on.
void unmirror(GameMap& map, GuiState& state, std::uint32_t id)
{
    auto it = state.players.find(id);
    if (it != state.players.end() && inBounds(map, it->second.x, it->second.y))
        map.removePlayerFromTile(it->second.x, it->second.y, id);
}

} // namespace

void ProtocolParser::apply(const std::string& line, GameMap& map, GuiState& state)
{
    std::istringstream iss(line);
    std::string        tag;
    if (!(iss >> tag))
        return;

    if (tag == "bct") {
        int x, y;
        if (!(iss >> x >> y) || !inBounds(map, x, y))
            return;
        for (int i = 0; i < MAP_RESOURCE_COUNT; ++i) {
            int q = 0;
            if (!(iss >> q))
                break;
            map.setResource(x, y, i, q);
        }
        return;
    }

    if (tag == "tna") {
        std::string name;
        if (iss >> name)
            state.teams.push_back(name);
        return;
    }

    if (tag == "sgt" || tag == "sst") {
        int f;
        if (iss >> f)
            state.frequency = f;
        return;
    }

    if (tag == "pnw") {
        std::string idtok, team;
        int         x, y, o, l;
        std::uint32_t id;
        if (!(iss >> idtok >> x >> y >> o >> l >> team) || !parseId(idtok, id))
            return;
        unmirror(map, state, id); // in case of a stale entry
        state.players[id] = PlayerInfo{x, y, o, l, team};
        if (inBounds(map, x, y))
            map.addPlayerToTile(x, y, id);
        return;
    }

    if (tag == "ppo") {
        std::string idtok;
        int         x, y, o;
        std::uint32_t id;
        if (!(iss >> idtok >> x >> y >> o) || !parseId(idtok, id))
            return;
        unmirror(map, state, id);
        auto& p = state.players[id]; // create if unknown (defensive)
        p.x = x; p.y = y; p.orient = o;
        if (inBounds(map, x, y))
            map.addPlayerToTile(x, y, id);
        return;
    }

    if (tag == "plv") {
        std::string idtok;
        int         l;
        std::uint32_t id;
        if (!(iss >> idtok >> l) || !parseId(idtok, id))
            return;
        auto it = state.players.find(id);
        if (it != state.players.end())
            it->second.level = l;
        return;
    }

    if (tag == "pdi") {
        std::string idtok;
        std::uint32_t id;
        if (!(iss >> idtok) || !parseId(idtok, id))
            return;
        unmirror(map, state, id);
        state.players.erase(id);
        return;
    }

    if (tag == "enw") {
        std::string eggtok, layertok;
        int         x, y;
        std::uint32_t egg;
        if (!(iss >> eggtok >> layertok >> x >> y) || !parseId(eggtok, egg))
            return;
        state.eggs[egg] = EggInfo{x, y};
        return;
    }

    if (tag == "ebo" || tag == "edi") {
        std::string eggtok;
        std::uint32_t egg;
        if (!(iss >> eggtok) || !parseId(eggtok, egg))
            return;
        state.eggs.erase(egg); // hatched into a player, or destroyed
        return;
    }

    if (tag == "pic") {
        int x, y, l;
        if (!(iss >> x >> y >> l) || !inBounds(map, x, y))
            return;
        state.incanting.insert(tileKey(map, x, y));
        return;
    }

    if (tag == "pie") {
        int x, y;
        if (!(iss >> x >> y) || !inBounds(map, x, y))
            return;
        state.incanting.erase(tileKey(map, x, y));
        return;
    }

    if (tag == "seg") {
        std::string name;
        if (iss >> name) {
            state.hasWinner = true;
            state.winner    = name;
        }
        return;
    }

    // msz: map already sized at construction. Everything else (pin/pgt/pdr/pfk/
    // pex/pbc/smg/suc/sbp) is cosmetic — bct/ppo already keep the model correct.
}
