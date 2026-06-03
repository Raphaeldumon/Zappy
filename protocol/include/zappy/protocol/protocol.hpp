#pragma once

// Umbrella header for the Zappy wire-protocol contract.
// Include this to pull in both the AI and GUI protocol definitions.

#include "zappy/protocol/ai_protocol.hpp"
#include "zappy/protocol/gui_protocol.hpp"

namespace zappy::protocol
{

// Bumped whenever the wire contract changes (with an ADR). Lets components refuse
// to talk to an incompatible peer if we ever add a version handshake.
inline constexpr int CONTRACT_VERSION = 1;

} // namespace zappy::protocol
