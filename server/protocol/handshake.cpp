#include "protocol/handshake.hpp"

namespace zappy::protocol
{

HandshakeResult handle_handshake(std::string_view team_name, const std::vector<core::Team> &teams,
                                 std::string &out_name)
{
    if (team_name == "GRAPHIC")
        return HandshakeResult::GUI;
    for (const auto &t : teams)
    {
        if (t.name == team_name)
        {
            out_name = t.name;
            return HandshakeResult::AI;
        }
    }
    return HandshakeResult::Invalid;
}

} // namespace zappy::protocol
