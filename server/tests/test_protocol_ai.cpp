#include "protocol/ai_handler.hpp"

#include <cassert>
#include <iostream>

using namespace zappy::protocol;
using Cmd = zappy::protocol::ai::Command;

static void test_parse_simple_commands()
{
    auto f = parse_ai_command("Forward");
    assert(f.has_value() && f->cmd == Cmd::Forward);

    auto r = parse_ai_command("Right");
    assert(r.has_value() && r->cmd == Cmd::Right);

    auto l = parse_ai_command("Left");
    assert(l.has_value() && l->cmd == Cmd::Left);

    auto look = parse_ai_command("Look");
    assert(look.has_value() && look->cmd == Cmd::Look);

    auto inv = parse_ai_command("Inventory");
    assert(inv.has_value() && inv->cmd == Cmd::Inventory);

    auto fork = parse_ai_command("Fork");
    assert(fork.has_value() && fork->cmd == Cmd::Fork);

    auto eject = parse_ai_command("Eject");
    assert(eject.has_value() && eject->cmd == Cmd::Eject);

    auto inc = parse_ai_command("Incantation");
    assert(inc.has_value() && inc->cmd == Cmd::Incantation);

    auto cn = parse_ai_command("Connect_nbr");
    assert(cn.has_value() && cn->cmd == Cmd::ConnectNbr);
}

static void test_parse_broadcast()
{
    auto b = parse_ai_command("Broadcast hello world");
    assert(b.has_value() && b->cmd == Cmd::Broadcast);
    assert(b->arg == "hello world");
}

static void test_parse_take_set()
{
    auto t = parse_ai_command("Take food");
    assert(t.has_value() && t->cmd == Cmd::Take);
    assert(t->resource_index == 0);

    auto t2 = parse_ai_command("Take linemate");
    assert(t2.has_value() && t2->resource_index == 1);

    auto s = parse_ai_command("Set thystame");
    assert(s.has_value() && s->cmd == Cmd::Set);
    assert(s->resource_index == 6);

    // Unknown resource
    auto bad = parse_ai_command("Take goldium");
    assert(!bad.has_value());
}

static void test_parse_unknown()
{
    auto u = parse_ai_command("fly");
    assert(!u.has_value());

    auto empty = parse_ai_command("");
    assert(!empty.has_value());
}

static void test_fmt_look()
{
    // Single tile, no contents
    std::vector<zappy::core::WorldState::LookTile> tiles(1);
    std::string result = fmt_look(tiles);
    assert(result == "[]");

    // One tile with food
    tiles[0].resources[0] = 2;
    result = fmt_look(tiles);
    assert(result == "[food food]");

    // One tile with player
    tiles[0] = {};
    tiles[0].player_count = 1;
    result = fmt_look(tiles);
    assert(result == "[player]");

    // Two tiles
    tiles.resize(2);
    tiles[0] = {};
    tiles[0].player_count = 1;
    tiles[1].resources[1] = 1; // linemate
    result = fmt_look(tiles);
    assert(result == "[player, linemate]");
}

static void test_fmt_inventory()
{
    zappy::core::ResourceSet inv{};
    inv[0] = 10; // food
    inv[1] = 2;  // linemate
    std::string result = fmt_inventory(inv);
    assert(result == "[food 10, linemate 2, deraumere 0, sibur 0, mendiane 0, phiras 0, thystame 0]");
}

static void test_fmt_connect_nbr()
{
    assert(fmt_connect_nbr(3) == "3");
    assert(fmt_connect_nbr(0) == "0");
}

int main()
{
    test_parse_simple_commands();
    test_parse_broadcast();
    test_parse_take_set();
    test_parse_unknown();
    test_fmt_look();
    test_fmt_inventory();
    test_fmt_connect_nbr();
    std::cout << "protocol_ai tests OK\n";
    return 0;
}
