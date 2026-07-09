#include "runtime/parse_args.hpp"
#include "runtime/server.hpp"

#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace
{
constexpr int EXIT_ERR = 84;

void sig_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
        zappy::runtime::Server::running_ = false;
}
} // namespace

int main(int argc, char **argv)
{
    std::optional<zappy::runtime::ServerArgs> parsed;
    try
    {
        parsed = zappy::runtime::parse_args(argc, argv);
    }
    catch (const std::exception &e)
    {
        std::cerr << "zappy_server : " << e.what() << "\n\n" << zappy::runtime::usage(argv[0]);
        return EXIT_ERR;
    }

    if (!parsed)
    {
        std::cout << zappy::runtime::usage(argv[0]);
        return EXIT_SUCCESS;
    }

    struct sigaction sa
    {
    };
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // no SA_RESTART: poll() returns EINTR immediately on Ctrl+C
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    try
    {
        zappy::runtime::Server server(*parsed);
        server.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "erreur fatale : " << e.what() << '\n';
        return EXIT_ERR;
    }
    return EXIT_SUCCESS;
}
