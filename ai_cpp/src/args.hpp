#pragma once

#include <optional>
#include <string>

namespace zappy::ai {

// AI client CLI per the subject:
//   ./zappy_ai -p <port> -n <team_name> -h <machine>
struct AiArgs {
    int port{4242};
    std::string team_name;
    std::string host{"localhost"};
};

[[nodiscard]] std::string usage(const char* prog);

// Returns std::nullopt on --help. Throws std::invalid_argument on bad input.
[[nodiscard]] std::optional<AiArgs> parse_args(int argc, char** argv);

} // namespace zappy::ai
