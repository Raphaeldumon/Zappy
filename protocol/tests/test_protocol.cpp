// Minimal contract sanity checks. Migrate to Catch2 once vcpkg deps are wired (ADR-006).
#include "zappy/protocol/protocol.hpp"

#include <cassert>
#include <iostream>

namespace ai = zappy::protocol::ai;

int main() {
    // Resource ordering is part of the wire contract (q0..q6).
    static_assert(ai::RESOURCE_COUNT == 7, "there are 7 resources");
    static_assert(ai::RESOURCE_NAMES[0] == "food", "q0 is food");
    static_assert(ai::RESOURCE_NAMES[6] == "thystame", "q6 is thystame");

    // Command time costs from the subject.
    assert(ai::time_cost(ai::Command::Forward) == 7);
    assert(ai::time_cost(ai::Command::Inventory) == 1);
    assert(ai::time_cost(ai::Command::Fork) == 42);
    assert(ai::time_cost(ai::Command::Incantation) == 300);
    assert(ai::time_cost(ai::Command::ConnectNbr) == 0);

    assert(zappy::protocol::CONTRACT_VERSION == 1);

    std::cout << "protocol contract OK\n";
    return 0;
}
