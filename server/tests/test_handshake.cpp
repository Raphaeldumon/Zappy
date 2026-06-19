// Handshake routing: GRAPHIC -> GUI, known team -> AI, anything else -> Invalid.
// Migrate to Catch2 once vcpkg is wired (ADR-006).
#include "protocol/handshake.hpp"

#include "core/world_state.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace zappy::core;
using zappy::protocol::handle_handshake;
using zappy::protocol::HandshakeResult;

namespace
{

// Two registered teams to match against.
std::vector<Team> make_teams()
{
    return {Team{0, "red", 5}, Team{1, "blue", 5}};
}

void test_graphic_routes_to_gui()
{
    auto teams = make_teams();
    std::string out;
    assert(handle_handshake("GRAPHIC", teams, out) == HandshakeResult::GUI);
}

void test_known_team_routes_to_ai()
{
    auto teams = make_teams();
    std::string out;
    assert(handle_handshake("red", teams, out) == HandshakeResult::AI);
    assert(out == "red");

    out.clear();
    assert(handle_handshake("blue", teams, out) == HandshakeResult::AI);
    assert(out == "blue");
}

void test_unknown_team_invalid()
{
    auto teams = make_teams();
    std::string out;
    assert(handle_handshake("green", teams, out) == HandshakeResult::Invalid);
    assert(handle_handshake("", teams, out) == HandshakeResult::Invalid);
    // Case-sensitive: "Red" is not "red".
    assert(handle_handshake("Red", teams, out) == HandshakeResult::Invalid);
}

void test_graphic_takes_priority_over_team()
{
    // Even if a team were somehow named GRAPHIC, the GUI branch wins. (parse_args
    // forbids that team name, but the handshake must still route GRAPHIC -> GUI.)
    std::vector<Team> teams = {Team{0, "GRAPHIC", 5}};
    std::string out;
    assert(handle_handshake("GRAPHIC", teams, out) == HandshakeResult::GUI);
}

} // namespace

int main()
{
    test_graphic_routes_to_gui();
    test_known_team_routes_to_ai();
    test_unknown_team_invalid();
    test_graphic_takes_priority_over_team();
    std::cout << "handshake tests OK\n";
    return 0;
}
