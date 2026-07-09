#include "runtime/parse_args.hpp"

#include "runtime/limits.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>

namespace zappy::runtime
{

std::string usage(const char *prog)
{
    std::ostringstream os;
    os << "USAGE: " << prog << " -p port -x width -y height -n nom1 nom2 ... -c clientsNb -f freq\n";
    os << "\n";
    os << "Options requises :\n";
    os << "  -p port        numéro de port\n";
    os << "  -x width       largeur de la carte\n";
    os << "  -y height      hauteur de la carte\n";
    os << "  -n nom...      nom(s) d'équipe\n";
    os << "  -c clientsNb   nombre de clients autorisés par équipe\n";
    os << "  -f freq        inverse de l'unité de temps pour l'exécution des actions\n";
    os << "\n";
    os << "Options :\n";
    os << "  --help         affiche cette aide\n";
    os << "\n";
    os << "Exemple :\n";
    os << "  " << prog << " -p 4242 -x 20 -y 20 -n rouge bleu -c 5 -f 100\n";
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
        throw std::invalid_argument("option " + flag + " attend un entier, reçu '" + std::string(value) + "'");
    }
}

const char *consume_value(int argc, char **argv, int &index, const std::string &flag)
{
    if (index + 1 >= argc)
    {
        throw std::invalid_argument("option " + flag + " attend une valeur");
    }
    return argv[++index];
}

void parse_team_names(int argc, char **argv, int &index, ServerArgs &args)
{
    while (index + 1 < argc && !is_option(argv[index + 1]))
    {
        std::string name = argv[++index];
        if (name.empty())
        {
            throw std::invalid_argument("un nom d'équipe ne peut pas être vide");
        }
        if (name == "GRAPHIC")
        {
            throw std::invalid_argument("le nom d'équipe 'GRAPHIC' est réservé à l'interface graphique");
        }
        if (std::find(args.teams.begin(), args.teams.end(), name) != args.teams.end())
        {
            throw std::invalid_argument("nom d'équipe en double : '" + name + "'");
        }
        if (static_cast<int>(args.teams.size()) >= MAX_TEAMS)
        {
            throw std::invalid_argument("trop d'équipes : " + std::to_string(MAX_TEAMS) + " au maximum");
        }
        args.teams.push_back(std::move(name));
    }
    if (args.teams.empty())
    {
        throw std::invalid_argument("l'option -n attend au moins un nom d'équipe");
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
        throw std::invalid_argument("options requises manquantes (il faut -p -x -y -n -c -f)");
    }
}

void validate_positive_values(const ServerArgs &args)
{
    if (args.width <= 0 || args.height <= 0 || args.clients_per_team <= 0 || args.frequency <= 0 || args.port <= 0)
    {
        throw std::invalid_argument("-p -x -y -c -f doivent tous être strictement positifs");
    }
    if (args.clients_per_team > MAX_CLIENTS_PER_TEAM)
    {
        throw std::invalid_argument("trop de clients par équipe : -c ne doit pas dépasser " +
                                    std::to_string(MAX_CLIENTS_PER_TEAM));
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
            throw std::invalid_argument("option inconnue : " + flag);
        }

        const char *value = consume_value(argc, argv, i, flag);
        parse_numeric_option(flag, value, args, required);
    }

    validate_required_options(args, required);
    validate_positive_values(args);
    return args;
}

} // namespace zappy::runtime
