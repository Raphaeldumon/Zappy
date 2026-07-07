#pragma once

#include "core/types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace zappy::protocol
{

// Static factory methods for every GUI protocol message.
// Each method returns the wire string WITHOUT a trailing \n.
// Pass the result to NetworkLayer::send_to() or broadcast_gui(),
// which call Client::enqueue() that appends \n automatically.
struct GuiEmitter
{
    // Snapshot messages
    [[nodiscard]] static std::string msz(int w, int h);
    [[nodiscard]] static std::string sgt(int frequency);
    [[nodiscard]] static std::string tna(std::string_view team_name);
    [[nodiscard]] static std::string bct(int x, int y, const core::ResourceSet &r);
    [[nodiscard]] static std::string pnw(core::PlayerId n, int x, int y, core::Orientation o, int level,
                                         std::string_view team_name);
    [[nodiscard]] static std::string enw(core::EggId e, core::PlayerId n, int x, int y);

    // Live events
    [[nodiscard]] static std::string ppo(core::PlayerId n, int x, int y, core::Orientation o);
    [[nodiscard]] static std::string plv(core::PlayerId n, int level);
    [[nodiscard]] static std::string pin(core::PlayerId n, int x, int y, const core::ResourceSet &inv);
    [[nodiscard]] static std::string pex(core::PlayerId n);
    [[nodiscard]] static std::string pbc(core::PlayerId n, std::string_view text);
    [[nodiscard]] static std::string pic(int x, int y, int level, const std::vector<core::PlayerId> &ids);
    [[nodiscard]] static std::string pie(int x, int y, bool success);
    [[nodiscard]] static std::string pfk(core::PlayerId n);
    [[nodiscard]] static std::string pdr(core::PlayerId n, int resource_index);
    [[nodiscard]] static std::string pgt(core::PlayerId n, int resource_index);
    [[nodiscard]] static std::string pdi(core::PlayerId n);
    [[nodiscard]] static std::string ebo(core::EggId e);
    [[nodiscard]] static std::string edi(core::EggId e);
    [[nodiscard]] static std::string sst(int frequency);
    [[nodiscard]] static std::string wth(std::string_view season, std::string_view weather, int duration_ticks);
    [[nodiscard]] static std::string seg(std::string_view team_name);
    [[nodiscard]] static std::string smg(std::string_view msg);
    [[nodiscard]] static std::string suc();
    [[nodiscard]] static std::string sbp();
};

} // namespace zappy::protocol
