#include "runtime/args.hpp"

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace zappy::runtime {

std::string usage(const char* prog) {
    std::ostringstream os;
    os << "USAGE: " << prog
       << " -p port -x width -y height -n name1 name2 ... -c clientsNb -f freq\n"
       << "  -p port        port number\n"
       << "  -x width       world width\n"
       << "  -y height      world height\n"
       << "  -n name...     team name(s)\n"
       << "  -c clientsNb   number of authorized clients per team\n"
       << "  -f freq        reciprocal of time unit for execution of actions\n"
       << "  --help         show this help\n";
    return os.str();
}

namespace {
int to_int(const std::string& flag, const char* value) {
    try {
        std::size_t pos = 0;
        int v = std::stoi(value, &pos);
        if (pos != std::strlen(value)) {
            throw std::invalid_argument("trailing characters");
        }
        return v;
    } catch (const std::exception&) {
        throw std::invalid_argument("option " + flag + " expects an integer, got '" +
                                    std::string(value) + "'");
    }
}
} // namespace

std::optional<ServerArgs> parse_args(int argc, char** argv) {
    ServerArgs args;
    bool got_port = false, got_x = false, got_y = false, got_c = false, got_f = false;

    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];

        if (flag == "--help" || flag == "-help" || flag == "-h") {
            return std::nullopt;
        }
        if (flag == "-n") {
            // Collect team names until the next flag.
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                args.teams.emplace_back(argv[++i]);
            }
            if (args.teams.empty()) {
                throw std::invalid_argument("option -n expects at least one team name");
            }
            continue;
        }
        if (i + 1 >= argc) {
            throw std::invalid_argument("option " + flag + " expects a value");
        }
        const char* val = argv[++i];
        if (flag == "-p") {
            args.port = to_int(flag, val);
            got_port = true;
        } else if (flag == "-x") {
            args.width = to_int(flag, val);
            got_x = true;
        } else if (flag == "-y") {
            args.height = to_int(flag, val);
            got_y = true;
        } else if (flag == "-c") {
            args.clients_per_team = to_int(flag, val);
            got_c = true;
        } else if (flag == "-f") {
            args.frequency = to_int(flag, val);
            got_f = true;
        } else {
            throw std::invalid_argument("unknown option: " + flag);
        }
    }

    if (!(got_port && got_x && got_y && got_c && got_f) || args.teams.empty()) {
        throw std::invalid_argument("missing required options (need -p -x -y -n -c -f)");
    }
    if (args.width <= 0 || args.height <= 0 || args.clients_per_team <= 0 || args.frequency <= 0 ||
        args.port <= 0) {
        throw std::invalid_argument("-p -x -y -c -f must all be positive");
    }
    return args;
}

} // namespace zappy::runtime
