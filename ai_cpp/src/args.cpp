#include "args.hpp"

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace zappy::ai
{

std::string usage(const char *prog)
{
    std::ostringstream os;
    os << "USAGE: " << prog << " -p port -n name -h machine\n"
       << "  -p port      port number\n"
       << "  -n name      name of the team\n"
       << "  -h machine   name of the machine; localhost by default\n"
       << "  --help       show this help\n";
    return os.str();
}

std::optional<AiArgs> parse_args(int argc, char **argv)
{
    AiArgs args;
    bool got_port = false, got_name = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string flag = argv[i];
        if (flag == "--help")
        {
            return std::nullopt;
        }
        if (i + 1 >= argc)
        {
            throw std::invalid_argument("option " + flag + " expects a value");
        }
        const char *val = argv[++i];
        if (flag == "-p")
        {
            try
            {
                std::size_t pos = 0;
                args.port = std::stoi(val, &pos);
                if (pos != std::strlen(val))
                {
                    throw std::invalid_argument("trailing chars");
                }
            }
            catch (const std::exception &)
            {
                throw std::invalid_argument("option -p expects an integer");
            }
            got_port = true;
        }
        else if (flag == "-n")
        {
            args.team_name = val;
            got_name = true;
        }
        else if (flag == "-h")
        {
            args.host = val;
        }
        else
        {
            throw std::invalid_argument("unknown option: " + flag);
        }
    }

    if (!got_port || !got_name)
    {
        throw std::invalid_argument("missing required options (need -p and -n)");
    }
    if (args.port <= 0)
    {
        throw std::invalid_argument("-p must be positive");
    }
    return args;
}

} // namespace zappy::ai
