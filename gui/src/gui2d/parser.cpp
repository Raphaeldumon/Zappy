#include "parser.hpp"

#include <charconv>
#include <cstdint>
#include <string>
#include <vector>

namespace zappy::gui2d
{
namespace
{

// Split a line on spaces into tokens. Empty tokens (double spaces) are dropped.
std::vector<std::string_view> tokenize(std::string_view line)
{
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < line.size())
    {
        while (i < line.size() && line[i] == ' ')
            ++i;
        std::size_t start = i;
        while (i < line.size() && line[i] != ' ')
            ++i;
        if (i > start)
            out.push_back(line.substr(start, i - start));
    }
    return out;
}

// Parse an int token, tolerating a leading '#'. Returns false on garbage.
bool to_int(std::string_view tok, int &out)
{
    if (!tok.empty() && tok.front() == '#')
        tok.remove_prefix(1);
    if (tok.empty())
        return false;
    auto res = std::from_chars(tok.data(), tok.data() + tok.size(), out);
    return res.ec == std::errc{};
}

bool to_id(std::string_view tok, std::uint32_t &out)
{
    int v = 0;
    if (!to_int(tok, v) || v < 0)
        return false;
    out = static_cast<std::uint32_t>(v);
    return true;
}

} // namespace

bool apply_line(World &world, std::string_view line)
{
    auto t = tokenize(line);
    if (t.empty())
        return true; // blank line, ignore
    std::string_view tag = t[0];

    // The server sends "WELCOME" to every client before the GUI snapshot. Not a
    // GUI-protocol tag, but expected — swallow it silently.
    if (tag == "WELCOME")
        return true;

    // msz X Y
    if (tag == "msz" && t.size() >= 3)
    {
        int w = 0, h = 0;
        if (to_int(t[1], w) && to_int(t[2], h))
        {
            world.resize(w, h);
            return true;
        }
        return false;
    }
    // sgt T / sst T
    if ((tag == "sgt" || tag == "sst") && t.size() >= 2)
        return to_int(t[1], world.time_unit);
    // tna N
    if (tag == "tna" && t.size() >= 2)
    {
        world.teams.emplace_back(t[1]);
        return true;
    }
    // bct X Y q0..q6
    if (tag == "bct" && t.size() >= 3 + RESOURCE_COUNT)
    {
        int x = 0, y = 0;
        if (!to_int(t[1], x) || !to_int(t[2], y))
            return false;
        Tile *tile = world.tile_at(x, y);
        if (!tile)
            return false;
        for (int i = 0; i < RESOURCE_COUNT; ++i)
            if (!to_int(t[3 + static_cast<std::size_t>(i)], tile->resources[i]))
                return false;
        return true;
    }
    // pnw n X Y O L N
    if (tag == "pnw" && t.size() >= 7)
    {
        std::uint32_t id = 0;
        Player p;
        if (!to_id(t[1], id) || !to_int(t[2], p.x) || !to_int(t[3], p.y) || !to_int(t[4], p.orientation) ||
            !to_int(t[5], p.level))
            return false;
        p.team = std::string(t[6]);
        world.players[id] = p;
        return true;
    }
    // ppo n X Y O
    if (tag == "ppo" && t.size() >= 5)
    {
        std::uint32_t id = 0;
        if (!to_id(t[1], id))
            return false;
        Player &p = world.players[id];
        return to_int(t[2], p.x) && to_int(t[3], p.y) && to_int(t[4], p.orientation);
    }
    // plv n L
    if (tag == "plv" && t.size() >= 3)
    {
        std::uint32_t id = 0;
        if (!to_id(t[1], id))
            return false;
        return to_int(t[2], world.players[id].level);
    }
    // pin n X Y q0..q6  (inventory — we only track position from it here)
    if (tag == "pin" && t.size() >= 3)
    {
        std::uint32_t id = 0;
        if (!to_id(t[1], id))
            return false;
        Player &p = world.players[id];
        return to_int(t[2], p.x) && to_int(t[3], p.y);
    }
    // pdi n / pex n  (death / expel — drop the player)
    if ((tag == "pdi" || tag == "pex") && t.size() >= 2)
    {
        std::uint32_t id = 0;
        if (!to_id(t[1], id))
            return false;
        if (tag == "pdi")
            world.players.erase(id);
        return true;
    }
    // enw e n X Y
    if (tag == "enw" && t.size() >= 5)
    {
        std::uint32_t id = 0;
        Egg e;
        if (!to_id(t[1], id) || !to_int(t[3], e.x) || !to_int(t[4], e.y))
            return false;
        world.eggs[id] = e;
        return true;
    }
    // ebo e / edi e  (egg hatched / died — remove it)
    if ((tag == "ebo" || tag == "edi") && t.size() >= 2)
    {
        std::uint32_t id = 0;
        if (!to_id(t[1], id))
            return false;
        world.eggs.erase(id);
        return true;
    }
    // smg M / seg N  (banner text)
    if ((tag == "smg" || tag == "seg") && t.size() >= 2)
    {
        world.last_message = std::string(line.substr(tag.size() + 1));
        return true;
    }

    // Tags we knowingly ignore in this debug GUI: pbc pic pie pfk pdr pgt suc sbp.
    if (tag == "pbc" || tag == "pic" || tag == "pie" || tag == "pfk" || tag == "pdr" || tag == "pgt" ||
        tag == "suc" || tag == "sbp")
        return true;

    return false; // genuinely unknown tag
}

} // namespace zappy::gui2d
