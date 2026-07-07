#include "protocol/gui_emitter.hpp"

#include <string>

namespace zappy::protocol
{

static int orient_wire(core::Orientation o) noexcept
{
    // N=1, E=2, S=3, W=4 as per GUI protocol
    switch (o)
    {
    case core::Orientation::North:
        return 1;
    case core::Orientation::East:
        return 2;
    case core::Orientation::South:
        return 3;
    case core::Orientation::West:
        return 4;
    }
    return 1;
}

static std::string resources_str(const core::ResourceSet &r)
{
    std::string s;
    for (std::size_t i = 0; i < r.size(); ++i)
    {
        if (i > 0)
            s += ' ';
        s += std::to_string(r[i]);
    }
    return s;
}

std::string GuiEmitter::msz(int w, int h)
{
    return "msz " + std::to_string(w) + ' ' + std::to_string(h);
}

std::string GuiEmitter::sgt(int frequency)
{
    return "sgt " + std::to_string(frequency);
}

std::string GuiEmitter::tna(std::string_view team_name)
{
    return "tna " + std::string(team_name);
}

std::string GuiEmitter::bct(int x, int y, const core::ResourceSet &r)
{
    return "bct " + std::to_string(x) + ' ' + std::to_string(y) + ' ' + resources_str(r);
}

std::string GuiEmitter::pnw(core::PlayerId n, int x, int y, core::Orientation o, int level, std::string_view team_name)
{
    return "pnw #" + std::to_string(n) + ' ' + std::to_string(x) + ' ' + std::to_string(y) + ' ' +
           std::to_string(orient_wire(o)) + ' ' + std::to_string(level) + ' ' + std::string(team_name);
}

std::string GuiEmitter::enw(core::EggId e, core::PlayerId n, int x, int y)
{
    return "enw #" + std::to_string(e) + " #" + std::to_string(n) + ' ' + std::to_string(x) + ' ' + std::to_string(y);
}

std::string GuiEmitter::ppo(core::PlayerId n, int x, int y, core::Orientation o)
{
    return "ppo #" + std::to_string(n) + ' ' + std::to_string(x) + ' ' + std::to_string(y) + ' ' +
           std::to_string(orient_wire(o));
}

std::string GuiEmitter::plv(core::PlayerId n, int level)
{
    return "plv #" + std::to_string(n) + ' ' + std::to_string(level);
}

std::string GuiEmitter::pin(core::PlayerId n, int x, int y, const core::ResourceSet &inv)
{
    return "pin #" + std::to_string(n) + ' ' + std::to_string(x) + ' ' + std::to_string(y) + ' ' + resources_str(inv);
}

std::string GuiEmitter::pex(core::PlayerId n)
{
    return "pex #" + std::to_string(n);
}

std::string GuiEmitter::pbc(core::PlayerId n, std::string_view text)
{
    return "pbc #" + std::to_string(n) + ' ' + std::string(text);
}

std::string GuiEmitter::pic(int x, int y, int level, const std::vector<core::PlayerId> &ids)
{
    std::string s = "pic " + std::to_string(x) + ' ' + std::to_string(y) + ' ' + std::to_string(level);
    for (auto id : ids)
        s += " #" + std::to_string(id);
    return s;
}

std::string GuiEmitter::pie(int x, int y, bool success)
{
    return "pie " + std::to_string(x) + ' ' + std::to_string(y) + ' ' + (success ? "1" : "0");
}

std::string GuiEmitter::pfk(core::PlayerId n)
{
    return "pfk #" + std::to_string(n);
}

std::string GuiEmitter::pdr(core::PlayerId n, int resource_index)
{
    return "pdr #" + std::to_string(n) + ' ' + std::to_string(resource_index);
}

std::string GuiEmitter::pgt(core::PlayerId n, int resource_index)
{
    return "pgt #" + std::to_string(n) + ' ' + std::to_string(resource_index);
}

std::string GuiEmitter::pdi(core::PlayerId n)
{
    return "pdi #" + std::to_string(n);
}

std::string GuiEmitter::ebo(core::EggId e)
{
    return "ebo #" + std::to_string(e);
}

std::string GuiEmitter::edi(core::EggId e)
{
    return "edi #" + std::to_string(e);
}

std::string GuiEmitter::sst(int frequency)
{
    return "sst " + std::to_string(frequency);
}

std::string GuiEmitter::wth(std::string_view season, std::string_view weather, int duration_ticks)
{
    return "wth " + std::string(season) + ' ' + std::string(weather) + ' ' + std::to_string(duration_ticks);
}

std::string GuiEmitter::seg(std::string_view team_name)
{
    return "seg " + std::string(team_name);
}

std::string GuiEmitter::smg(std::string_view msg)
{
    return "smg " + std::string(msg);
}

std::string GuiEmitter::suc()
{
    return "suc";
}
std::string GuiEmitter::sbp()
{
    return "sbp";
}

} // namespace zappy::protocol
