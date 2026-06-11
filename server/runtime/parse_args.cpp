#include "runtime/parse_args.hpp"

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace zappy::runtime
{

std::string usage(const char *prog)
{
    std::ostringstream os;
    os << "USAGE: " << prog << " -p port -x width -y height -n name1 name2 ... -c clientsNb -f freq\n"
       << "  -p port        port number\n"
       << "  -x width       world width\n"
       << "  -y height      world height\n"
       << "  -n name...     team name(s)\n"
       << "  -c clientsNb   number of authorized clients per team\n"
       << "  -f freq        reciprocal of time unit for execution of actions\n"
       << "  --help         show this help\n";
    return os.str();
}

namespace
{

struct RequiredOptions
{
    bool port{false};
    bool width{false};
    bool height{false};
    bool clients{false};
    bool frequency{false};
};

bool is_help_flag(const std::string &flag)
{
    return flag == "--help" || flag == "-help" || flag == "-h";
}

bool is_option(const char *arg)
{
    return arg != nullptr && arg[0] == '-';
}

int to_int(const std::string &flag, const char *value)
{
    try
    {
        std::size_t pos = 0;
        int v = std::stoi(value, &pos);
        if (pos != std::strlen(value))
        {
            throw std::invalid_argument("trailing characters");
        }
        return v;
    }
    catch (const std::exception &)
    {
        throw std::invalid_argument("option " + flag + " expects an integer, got '" + std::string(value) + "'");
    }
}

const char *consume_value(int argc, char **argv, int &index, const std::string &flag)
{
    if (index + 1 >= argc)
    {
        throw std::invalid_argument("option " + flag + " expects a value");
    }
    return argv[++index];
}

void parse_team_names(int argc, char **argv, int &index, ServerArgs &args)
{
    while (index + 1 < argc && !is_option(argv[index + 1]))
    {
        args.teams.emplace_back(argv[++index]);
    }
    if (args.teams.empty())
    {
        throw std::invalid_argument("option -n expects at least one team name");
    }
}

bool is_numeric_option(const std::string &flag)
{
    return flag == "-p" || flag == "-x" || flag == "-y" || flag == "-c" || flag == "-f";
}

void parse_numeric_option(const std::string &flag, const char *value, ServerArgs &args, RequiredOptions &required)
{
    if (flag == "-p")
    {
        args.port = to_int(flag, value);
        required.port = true;
        return;
    }
    if (flag == "-x")
    {
        args.width = to_int(flag, value);
        required.width = true;
        return;
    }
    if (flag == "-y")
    {
        args.height = to_int(flag, value);
        required.height = true;
        return;
    }
    if (flag == "-c")
    {
        args.clients_per_team = to_int(flag, value);
        required.clients = true;
        return;
    }
    if (flag == "-f")
    {
        args.frequency = to_int(flag, value);
        required.frequency = true;
        return;
    }
}

void validate_required_options(const ServerArgs &args, const RequiredOptions &required)
{
    if (!required.port || !required.width || !required.height || !required.clients || !required.frequency ||
        args.teams.empty())
    {
        throw std::invalid_argument("missing required options (need -p -x -y -n -c -f)");
    }
}

void validate_positive_values(const ServerArgs &args)
{
    if (args.width <= 0 || args.height <= 0 || args.clients_per_team <= 0 || args.frequency <= 0 || args.port <= 0)
    {
        throw std::invalid_argument("-p -x -y -c -f must all be positive");
    }
}

} // namespace

std::optional<ServerArgs> parse_args(int argc, char **argv)
{
    ServerArgs args;
    RequiredOptions required;

    for (int i = 1; i < argc; ++i)
    {
        std::string flag = argv[i];

        if (is_help_flag(flag))
        {
            return std::nullopt;
        }
        if (flag == "-n")
        {
            parse_team_names(argc, argv, i, args);
            continue;
        }

        if (!is_numeric_option(flag))
        {
            throw std::invalid_argument("unknown option: " + flag);
        }

        const char *value = consume_value(argc, argv, i, flag);
        parse_numeric_option(flag, value, args, required);
    }

    validate_required_options(args, required);
    validate_positive_values(args);
    return args;
}

} // namespace zappy::runtime
