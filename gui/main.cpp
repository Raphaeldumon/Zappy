#include "interface.hpp"
#include "netClient.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

namespace {

void usage(const char* prog)
{
    std::cerr << "USAGE: " << prog << " -p port [-h machine]\n"
              << "  -p port      port number\n"
              << "  -h machine   server host (default 127.0.0.1)\n";
}

} // namespace

int main(int argc, char** argv)
{
    int         port = -1;
    std::string host = "127.0.0.1";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else {
            usage(argv[0]);
            return 84;
        }
    }

    if (port <= 0) {
        usage(argv[0]);
        return 84;
    }

    auto net = std::make_unique<NetClient>();
    if (!net->connect(host, port)) {
        std::cerr << "Error: cannot connect to " << host << ':' << port << '\n';
        return 84;
    }

    int width = 0, height = 0;
    if (!net->handshake(width, height)) {
        std::cerr << "Error: GRAPHIC handshake failed\n";
        return 84;
    }

    try {
        Interface app(std::move(net), width, height,
                      DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 84;
    }
    return 0;
}
