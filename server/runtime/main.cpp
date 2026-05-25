// zappy_server entry point.
//
// S1 scope: parse the CLI, build the WorldState, print a banner. The poll/asio
// network loop (P2) and the tick loop (P1) plug in where marked below.

#include "core/world_state.hpp"
#include "runtime/args.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

namespace {
constexpr int EXIT_ERR = 84; // Epitech error code
}

int main(int argc, char** argv) {
    std::optional<zappy::runtime::ServerArgs> parsed;
    try {
        parsed = zappy::runtime::parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "zappy_server: " << e.what() << "\n\n" << zappy::runtime::usage(argv[0]);
        return EXIT_ERR;
    }

    if (!parsed) { // --help
        std::cout << zappy::runtime::usage(argv[0]);
        return EXIT_SUCCESS;
    }

    const auto& a = *parsed;
    zappy::core::WorldState world(a.width, a.height);

    std::cout << "zappy_server v0.0.1\n"
              << "  port    : " << a.port << '\n'
              << "  world   : " << world.width() << " x " << world.height() << '\n'
              << "  freq    : " << a.frequency << '\n'
              << "  clients : " << a.clients_per_team << " per team\n"
              << "  teams   :";
    for (const auto& t : a.teams) {
        std::cout << ' ' << t;
    }
    std::cout << "\nhello from zappy_server\n";

    // TODO(P2): NetworkLayer accept/poll loop on `a.port` (ADR-009 asio).
    // TODO(P1): EventScheduler-driven tick loop at frequency `a.frequency`.
    return EXIT_SUCCESS;
}
