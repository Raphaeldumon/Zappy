// CLI parsing tests, focused on team-name validation (GRAPHIC reserved,
// duplicates, empty) plus the basic happy path and required-option checks.
// Migrate to Catch2 once vcpkg is wired (ADR-006).
#include "runtime/parse_args.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using zappy::runtime::parse_args;
using zappy::runtime::ServerArgs;

namespace
{

// Build a mutable argv from string literals (parse_args takes char**).
struct Args
{
    std::vector<std::string> storage;
    std::vector<char *> argv;

    explicit Args(std::vector<std::string> a) : storage(std::move(a))
    {
        for (auto &s : storage)
            argv.push_back(s.data());
    }
    int argc() const
    {
        return static_cast<int>(argv.size());
    }
    char **data()
    {
        return argv.data();
    }
};

// Returns true if parse_args threw std::invalid_argument.
bool rejects(std::vector<std::string> a)
{
    Args args(std::move(a));
    try
    {
        auto result = parse_args(args.argc(), args.data());
        (void)result;
        return false;
    }
    catch (const std::invalid_argument &)
    {
        return true;
    }
}

void test_happy_path()
{
    Args a({"zappy_server", "-p", "4242", "-x", "10", "-y", "10", "-n", "red", "blue", "-c", "5", "-f", "100"});
    auto parsed = parse_args(a.argc(), a.data());
    assert(parsed.has_value());
    assert(parsed->port == 4242);
    assert(parsed->width == 10 && parsed->height == 10);
    assert(parsed->clients_per_team == 5);
    assert(parsed->frequency == 100);
    assert(parsed->teams.size() == 2);
    assert(parsed->teams[0] == "red" && parsed->teams[1] == "blue");
}

void test_help_returns_nullopt()
{
    Args a({"zappy_server", "--help"});
    auto parsed = parse_args(a.argc(), a.data());
    assert(!parsed.has_value());
}

void test_graphic_team_rejected()
{
    // GRAPHIC is reserved for the GUI handshake — never a real team.
    assert(rejects({"zappy_server", "-p", "4242", "-x", "10", "-y", "10", "-n", "GRAPHIC", "-c", "5", "-f", "100"}));
    assert(rejects({"zappy_server", "-p", "4242", "-x", "10", "-y", "10", "-n", "red", "GRAPHIC", "-c", "5", "-f",
                    "100"}));
}

void test_duplicate_team_rejected()
{
    assert(rejects({"zappy_server", "-p", "4242", "-x", "10", "-y", "10", "-n", "red", "red", "-c", "5", "-f", "100"}));
}

void test_empty_team_rejected()
{
    assert(rejects({"zappy_server", "-p", "4242", "-x", "10", "-y", "10", "-n", "", "-c", "5", "-f", "100"}));
}

void test_too_many_teams_rejected()
{
    // MAX_TEAMS is 8; nine distinct names must be rejected.
    assert(rejects({"zappy_server", "-p", "4242", "-x", "10", "-y", "10", "-n", "t1", "t2", "t3", "t4", "t5", "t6",
                    "t7", "t8", "t9", "-c", "5", "-f", "100"}));
}

void test_missing_and_invalid()
{
    // No -n at all.
    assert(rejects({"zappy_server", "-p", "4242", "-x", "10", "-y", "10", "-c", "5", "-f", "100"}));
    // Non-numeric port.
    assert(rejects({"zappy_server", "-p", "abc", "-x", "10", "-y", "10", "-n", "red", "-c", "5", "-f", "100"}));
    // Non-positive dimension.
    assert(rejects({"zappy_server", "-p", "4242", "-x", "0", "-y", "10", "-n", "red", "-c", "5", "-f", "100"}));
}

} // namespace

int main()
{
    test_happy_path();
    test_help_returns_nullopt();
    test_graphic_team_rejected();
    test_duplicate_team_rejected();
    test_empty_team_rejected();
    test_too_many_teams_rejected();
    test_missing_and_invalid();
    std::cout << "parse_args OK\n";
    return 0;
}
