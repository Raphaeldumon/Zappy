#pragma once

#include <string_view>

namespace zappy::protocol
{

enum class GuiRequest
{
    Msz,      // msz
    Mct,      // mct
    Tna,      // tna
    Bct,      // bct X Y
    Ppo,      // ppo #n
    Plv,      // plv #n
    Pin,      // pin #n
    Sgt,      // sgt
    Sst,      // sst T
    Unknown,  // unrecognized tag → suc
    BadParam, // recognized tag, bad parameters → sbp
};

struct ParsedGuiRequest
{
    GuiRequest type{GuiRequest::Unknown};
    int x{-1}, y{-1}; // for bct X Y
    int n{-1};        // for ppo/plv/pin #n
    int t{-1};        // for sst T
};

[[nodiscard]] ParsedGuiRequest parse_gui_request(std::string_view line);

} // namespace zappy::protocol
