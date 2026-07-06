#include "protocolParser.hpp"

#include "gameMap.hpp"
#include "guiState.hpp"

#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace
{

// "#3" -> 3, "3" -> 3. Returns false on garbage.
bool parseId(const std::string &tok, std::uint32_t &out)
{
    const char *s = tok.c_str();
    if (*s == '#')
        ++s;
    if (*s == '\0')
        return false;
    char *end = nullptr;
    unsigned long v = std::strtoul(s, &end, 10);
    if (end == s || *end != '\0')
        return false;
    out = static_cast<std::uint32_t>(v);
    return true;
}

bool inBounds(const GameMap &map, int x, int y)
{
    return x >= 0 && y >= 0 && x < map.getWidth() && y < map.getHeight();
}

long long tileKey(const GameMap &map, int x, int y)
{
    return static_cast<long long>(y) * map.getWidth() + x;
}

// Drop a player from whatever tile the registry says it's on.
void unmirror(GameMap &map, GuiState &state, std::uint32_t id)
{
    auto it = state.players.find(id);
    if (it != state.players.end() && inBounds(map, it->second.getX(), it->second.getY()))
        map.removePlayerFromTile(it->second.getX(), it->second.getY(), it->second);
}

void setWinnerIfLevel8(const aiPlayer &player, GuiState &state)
{
    if (state.hasWinner || player.getLevel() < 8)
        return;
    state.hasWinner = true;
    state.winner = player.getTeam();
}

} // namespace

void ProtocolParser::apply(const std::string &line, GameMap &map, GuiState &state)
{
    std::istringstream iss(line);
    std::string tag;
    if (!(iss >> tag))
        return;

    if (tag == "bct")
    {
        int x, y;
        if (!(iss >> x >> y) || !inBounds(map, x, y))
            return;
        for (int i = 0; i < MAP_RESOURCE_COUNT; ++i)
        {
            int q = 0;
            if (!(iss >> q))
                break;
            map.setResource(x, y, i, q);
        }
        return;
    }

    if (tag == "tna")
    {
        std::string name;
        if (iss >> name)
            state.teams.push_back(name);
        return;
    }

    if (tag == "sgt" || tag == "sst")
    {
        int f;
        if (iss >> f)
            state.frequency = f;
        return;
    }

    if (tag == "pnw")
    {
        std::string idtok, team;
        int x, y, o, l;
        std::uint32_t id;
        if (!(iss >> idtok >> x >> y >> o >> l >> team) || !parseId(idtok, id))
            return;
        const bool firstSeen = state.players.find(id) == state.players.end();
        unmirror(map, state, id); // in case of a stale entry
        // aiPlayer has const id/team and no assignment, so replace by erase+emplace.
        state.players.erase(id);
        auto [it, ok] = state.players.emplace(id, aiPlayer(id, team, x, y, static_cast<Orientation>(o)));
        if (ok)
        {
            it->second.setLevel(l);
            setWinnerIfLevel8(it->second, state);
        }
        if (inBounds(map, x, y))
            map.addPlayerToTile(x, y, it->second);
        if (firstSeen)
            state.feedEvents.push_back({GameEventKind::Join, id, x, y, l, team});
        return;
    }

    if (tag == "ppo")
    {
        std::string idtok;
        int x, y, o;
        std::uint32_t id;
        if (!(iss >> idtok >> x >> y >> o) || !parseId(idtok, id))
            return;
        auto it = state.players.find(id);
        if (it == state.players.end())
            return; // unknown player; a pnw will define it
        unmirror(map, state, id);
        it->second.changePosition(x, y);
        it->second.changeOrientation(static_cast<Orientation>(o));
        if (inBounds(map, x, y))
            map.addPlayerToTile(x, y, it->second);
        return;
    }

    if (tag == "plv")
    {
        std::string idtok;
        int l;
        std::uint32_t id;
        if (!(iss >> idtok >> l) || !parseId(idtok, id))
            return;
        auto it = state.players.find(id);
        if (it != state.players.end())
        {
            const bool levelledUp = l > it->second.getLevel();
            it->second.setLevel(l);
            setWinnerIfLevel8(it->second, state);
            if (inBounds(map, it->second.getX(), it->second.getY()))
            {
                map.removePlayerFromTile(it->second.getX(), it->second.getY(), it->second);
                map.addPlayerToTile(it->second.getX(), it->second.getY(), it->second);
            }
            if (levelledUp)
                state.feedEvents.push_back(
                    {GameEventKind::LevelUp, id, it->second.getX(), it->second.getY(), l, it->second.getTeam()});
        }
        return;
    }

    if (tag == "pdi")
    {
        std::string idtok;
        std::uint32_t id;
        if (!(iss >> idtok) || !parseId(idtok, id))
            return;
        // Capture the last pose before erasing so the renderer can play a death
        // animation at the spot the player vanished from (it is gone after this).
        auto it = state.players.find(id);
        if (it != state.players.end())
        {
            state.animEvents.push_back(
                {id, PlayerAnimEventKind::Death, it->second.getX(), it->second.getY(), it->second.getOrientation()});
            state.feedEvents.push_back({GameEventKind::Death, id, it->second.getX(), it->second.getY(),
                                        it->second.getLevel(), it->second.getTeam()});
        }
        unmirror(map, state, id);
        state.players.erase(id);
        return;
    }

    // Action packets the model already reflects via bct/ppo, but which we surface
    // as one-shot animations: eject (pex), broadcast (pbc), collect (pgt).
    if (tag == "pex" || tag == "pbc" || tag == "pgt")
    {
        std::string idtok;
        std::uint32_t id;
        if (!(iss >> idtok) || !parseId(idtok, id))
            return;
        auto it = state.players.find(id);
        if (it == state.players.end())
            return;
        const PlayerAnimEventKind kind = tag == "pex"   ? PlayerAnimEventKind::Kick
                                         : tag == "pbc" ? PlayerAnimEventKind::Jump
                                                        : PlayerAnimEventKind::Pickup;
        state.animEvents.push_back({id, kind, it->second.getX(), it->second.getY(), it->second.getOrientation()});
        if (tag == "pbc")
        {
            // The message is the rest of the line, verbatim (may contain spaces).
            std::string msg;
            std::getline(iss, msg);
            if (!msg.empty() && msg.front() == ' ')
                msg.erase(0, 1);
            state.feedEvents.push_back(
                {GameEventKind::Broadcast, id, it->second.getX(), it->second.getY(), 0, msg});
        }
        else if (tag == "pex")
        {
            state.feedEvents.push_back(
                {GameEventKind::Eject, id, it->second.getX(), it->second.getY(), 0, it->second.getTeam()});
        }
        return;
    }

    if (tag == "enw")
    {
        std::string eggtok, layertok;
        int x, y;
        std::uint32_t egg;
        if (!(iss >> eggtok >> layertok >> x >> y) || !parseId(eggtok, egg))
            return;
        state.eggs[egg] = EggInfo{x, y};
        return;
    }

    if (tag == "ebo" || tag == "edi")
    {
        std::string eggtok;
        std::uint32_t egg;
        if (!(iss >> eggtok) || !parseId(eggtok, egg))
            return;
        state.eggs.erase(egg); // hatched into a player, or destroyed
        return;
    }

    if (tag == "pic")
    {
        int x, y, l;
        if (!(iss >> x >> y >> l) || !inBounds(map, x, y))
            return;
        state.incanting.insert(tileKey(map, x, y));
        state.feedEvents.push_back({GameEventKind::IncantStart, 0, x, y, l, {}});
        return;
    }

    if (tag == "pie")
    {
        int x, y;
        if (!(iss >> x >> y) || !inBounds(map, x, y))
            return;
        state.incanting.erase(tileKey(map, x, y));
        int result = -1; // servers send 0/1; tolerate its absence
        iss >> result;
        state.feedEvents.push_back({GameEventKind::IncantEnd, 0, x, y, result, {}});
        return;
    }

    if (tag == "pfk")
    {
        std::string idtok;
        std::uint32_t id;
        if (!(iss >> idtok) || !parseId(idtok, id))
            return;
        auto it = state.players.find(id);
        if (it != state.players.end())
            state.feedEvents.push_back(
                {GameEventKind::Fork, id, it->second.getX(), it->second.getY(), 0, it->second.getTeam()});
        return;
    }

    if (tag == "seg")
    {
        std::string name;
        if (iss >> name)
        {
            state.hasWinner = true;
            state.winner = name;
            state.feedEvents.push_back({GameEventKind::Win, 0, 0, 0, 0, name});
        }
        return;
    }

    // msz: map already sized at construction. Everything else (pin/pdr/
    // smg/suc/sbp) is cosmetic — bct/ppo already keep the model correct.
}
