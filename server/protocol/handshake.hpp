#pragma once

#include "core/types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace zappy::protocol
{

enum class HandshakeResult
{
    GUI,
    AI,
    Invalid
};

// Classify the team-name line sent by a newly connected client.
// Returns GUI if name == "GRAPHIC", AI if name matches a registered team,
// Invalid otherwise.
// out_name is populated with the matched team name on AI result.
HandshakeResult handle_handshake(std::string_view team_name, const std::vector<core::Team> &teams,
                                 std::string &out_name);

} // namespace zappy::protocol
