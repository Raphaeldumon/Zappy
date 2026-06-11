#pragma once

#include <optional>
#include <string>
#include <vector>

namespace zappy::runtime
{

// Parsed server CLI per the subject:
//   ./zappy_server -p <port> -x <width> -y <height> -n <team1> [team2 ...] -c <clients> -f <freq>
struct ServerArgs
{
    int port{4242};
    int width{10};
    int height{10};
    std::vector<std::string> teams;
    int clients_per_team{1};
    int frequency{100};
};

// Usage string printed by --help / -help.
[[nodiscard]] std::string usage(const char *prog);

// Parse argv. Returns:
//   - parsed args on success
//   - std::nullopt if --help was requested (caller prints usage, exits 0)
// Throws std::invalid_argument on malformed input (caller prints error, exits 84).
[[nodiscard]] std::optional<ServerArgs> parse_args(int argc, char **argv);

} // namespace zappy::runtime
