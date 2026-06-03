#pragma once

#include "zappy/protocol/ai_protocol.hpp"

#include <string>

namespace zappy::ai
{

// Decision-making interface for an AI drone. The S1 stub returns a fixed action;
// the trained policy (libtorch, loaded from models/current/model.pt) implements
// this later. A rule-based fallback also implements it for the no-checkpoint case.
class Agent
{
  public:
    virtual ~Agent() = default;

    // Given the latest observation (raw server lines for now), choose the next
    // command to send. P5/P6 will replace the string obs with an encoded tensor.
    [[nodiscard]] virtual protocol::ai::Command decide(const std::string &observation) = 0;
};

} // namespace zappy::ai
