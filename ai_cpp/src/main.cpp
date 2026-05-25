// zappy_ai entry point.
//
// S1 scope: parse the CLI and print a banner. The socket client + handshake
// (WELCOME -> team -> CLIENT_NUM -> X Y) and the policy come next (P5/P6).

#include "args.hpp"
#include "zappy/protocol/ai_protocol.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

namespace {
constexpr int EXIT_ERR = 84;
}

int main(int argc, char** argv) {
    std::optional<zappy::ai::AiArgs> parsed;
    try {
        parsed = zappy::ai::parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "zappy_ai: " << e.what() << "\n\n" << zappy::ai::usage(argv[0]);
        return EXIT_ERR;
    }

    if (!parsed) {
        std::cout << zappy::ai::usage(argv[0]);
        return EXIT_SUCCESS;
    }

    const auto& a = *parsed;
    std::cout << "zappy_ai v0.0.1\n"
              << "  host : " << a.host << '\n'
              << "  port : " << a.port << '\n'
              << "  team : " << a.team_name << '\n'
              << "hello from zappy_ai\n";

    // TODO(P6): connect socket to a.host:a.port, run the AI handshake.
    // TODO(P5): load policy from models/current/model.pt (or rule-based fallback),
    //           loop: read server lines -> Agent::decide -> send command.
    return EXIT_SUCCESS;
}
