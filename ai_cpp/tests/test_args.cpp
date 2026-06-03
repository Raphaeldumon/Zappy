// Skeleton unit tests for the AI client CLI. Migrate to Catch2 later (ADR-006).
#include "args.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace zappy::ai;

static std::optional<AiArgs> run(std::vector<const char *> argv)
{
    return parse_args(static_cast<int>(argv.size()), const_cast<char **>(argv.data()));
}

static void test_valid()
{
    auto a = run({"zappy_ai", "-p", "4242", "-n", "red", "-h", "example.com"});
    assert(a.has_value());
    assert(a->port == 4242);
    assert(a->team_name == "red");
    assert(a->host == "example.com");
}

static void test_default_host()
{
    auto a = run({"zappy_ai", "-p", "4242", "-n", "blue"});
    assert(a.has_value());
    assert(a->host == "localhost");
}

static void test_help_returns_nullopt()
{
    auto a = run({"zappy_ai", "--help"});
    assert(!a.has_value());
}

static void test_missing_required_throws()
{
    bool threw = false;
    try
    {
        run({"zappy_ai", "-p", "4242"}); // no -n
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    assert(threw);
}

int main()
{
    test_valid();
    test_default_host();
    test_help_returns_nullopt();
    test_missing_required_throws();
    std::cout << "ai args tests OK\n";
    return 0;
}
