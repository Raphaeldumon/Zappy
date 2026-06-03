// zappy_gui entry point.
//
// S1 scope: parse the CLI (-p -n GRAPHIC -h), then either open a Vulkan window
// (when built with ZAPPY_GUI_HAS_VULKAN) or print a banner (stub build).
// The GUI connects as a "GRAPHIC" client and speaks the GUI protocol.

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#if defined(ZAPPY_GUI_HAS_VULKAN)
namespace zappy::gui
{
int run_vulkan_app(); // defined in renderer/vulkan_app.cpp
}
#endif

namespace
{
constexpr int EXIT_ERR = 84;

void print_usage(const char *prog)
{
    std::cout << "USAGE: " << prog << " -p port -h machine\n"
              << "  -p port      port number\n"
              << "  -h machine   server machine; localhost by default\n"
              << "  --help       show this help\n";
}
} // namespace

int main(int argc, char **argv)
{
    int port = 4242;
    std::string host = "localhost";

    for (int i = 1; i < argc; ++i)
    {
        std::string flag = argv[i];
        if (flag == "--help")
        {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (i + 1 >= argc)
        {
            std::cerr << "zappy_gui: option " << flag << " expects a value\n";
            return EXIT_ERR;
        }
        const char *val = argv[++i];
        if (flag == "-p")
        {
            port = std::atoi(val);
        }
        else if (flag == "-h")
        {
            host = val;
        }
        else
        {
            std::cerr << "zappy_gui: unknown option " << flag << "\n";
            return EXIT_ERR;
        }
    }

    std::cout << "zappy_gui v0.0.1 (server " << host << ":" << port << ")\n";

#if defined(ZAPPY_GUI_HAS_VULKAN)
    return zappy::gui::run_vulkan_app();
#else
    std::cout << "hello from zappy_gui (stub build — no Vulkan/glfw3 found)\n"
              << "Install the Vulkan SDK + glfw3 and run `make gui_on` for the window.\n";
    // TODO(P4): connect to host:port as GRAPHIC, parse GUI protocol lines.
    return EXIT_SUCCESS;
#endif
}
