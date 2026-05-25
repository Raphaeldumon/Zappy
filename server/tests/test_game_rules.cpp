#include "core/game_rules.hpp"
#include "core/world_state.hpp"

#include <cassert>
#include <iostream>

using namespace zappy::core;

static void test_broadcast_direction_same_tile()
{
    assert(broadcast_direction(3, 3, 3, 3, Orientation::North, 10, 10) == 0);
}

static void test_broadcast_direction_north_facing()
{
    // Receiver at (5,5) facing North; sender directly ahead → K=1
    assert(broadcast_direction(5, 4, 5, 5, Orientation::North, 10, 10) == 1);
    // Sender directly behind → K=5
    assert(broadcast_direction(5, 6, 5, 5, Orientation::North, 10, 10) == 5);
    // Sender to right (East) → K=3
    assert(broadcast_direction(6, 5, 5, 5, Orientation::North, 10, 10) == 3);
    // Sender to left (West) → K=7
    assert(broadcast_direction(4, 5, 5, 5, Orientation::North, 10, 10) == 7);
}

static void test_broadcast_direction_east_facing()
{
    // Receiver at (5,5) facing East; sender to right (+Y) → K=3
    assert(broadcast_direction(5, 6, 5, 5, Orientation::East, 10, 10) == 3);
    // Sender ahead (+X) → K=1
    assert(broadcast_direction(6, 5, 5, 5, Orientation::East, 10, 10) == 1);
}

static void test_broadcast_direction_toroidal()
{
    // World 10x10; sender at x=0, receiver at x=9, facing East: shortest path is x=-1
    // dx = 0-9 = -9, adjust: -9 < -(10/2)=-5 → dx = -9+10 = 1 (sender is ahead of receiver)
    assert(broadcast_direction(0, 5, 9, 5, Orientation::East, 10, 10) == 1);
}

static void test_broadcast_direction_diagonals()
{
    // Receiver at (5,5) facing North; sender at (6,4) — front-right → K=2
    assert(broadcast_direction(6, 4, 5, 5, Orientation::North, 10, 10) == 2);
    // Sender at (4,4) — front-left → K=8
    assert(broadcast_direction(4, 4, 5, 5, Orientation::North, 10, 10) == 8);
    // Sender at (6,6) — back-right → K=4
    assert(broadcast_direction(6, 6, 5, 5, Orientation::North, 10, 10) == 4);
    // Sender at (4,6) — back-left → K=6
    assert(broadcast_direction(4, 6, 5, 5, Orientation::North, 10, 10) == 6);
}

static void test_can_elevate_level1()
{
    WorldState w(5, 5);
    w.register_team(0, "team", 5);

    // Need 1 player, 1 linemate
    auto &p = w.add_player(0, 2, 2, Orientation::North);
    p.level = 1;

    // No linemate yet → fail
    assert(!can_elevate(w, 2, 2, 1));

    // Add linemate (resource index 1)
    w.at(2, 2).resources[1] = 1;
    assert(can_elevate(w, 2, 2, 1));
}

static void test_can_elevate_level2()
{
    WorldState w(5, 5);
    w.register_team(0, "team", 5);

    // Need 2 players, linemate+deraumere+sibur
    auto &p1 = w.add_player(0, 2, 2, Orientation::North);
    p1.level = 2;
    auto &p2 = w.add_player(0, 2, 2, Orientation::North);
    p2.level = 2;

    // Only 1 player is wrong level check (need 2)
    w.at(2, 2).resources[1] = 1; // linemate
    w.at(2, 2).resources[2] = 1; // deraumere
    w.at(2, 2).resources[3] = 1; // sibur

    assert(can_elevate(w, 2, 2, 2));

    // Remove one player by marking dead
    p2.alive = false;
    assert(!can_elevate(w, 2, 2, 2));
}

static void test_consume_elevation_stones()
{
    WorldState w(5, 5);
    w.register_team(0, "team", 5);

    auto &p = w.add_player(0, 2, 2, Orientation::North);
    p.level = 1;
    w.at(2, 2).resources[1] = 3; // linemate

    consume_elevation_stones(w, 2, 2, 1);

    // Should have consumed 1 linemate
    assert(w.at(2, 2).resources[1] == 2);
}

static void test_can_elevate_bad_level()
{
    WorldState w(5, 5);
    assert(!can_elevate(w, 0, 0, 0));
    assert(!can_elevate(w, 0, 0, 8));
}

int main()
{
    test_broadcast_direction_same_tile();
    test_broadcast_direction_north_facing();
    test_broadcast_direction_east_facing();
    test_broadcast_direction_toroidal();
    test_broadcast_direction_diagonals();
    test_can_elevate_level1();
    test_can_elevate_level2();
    test_consume_elevation_stones();
    test_can_elevate_bad_level();
    std::cout << "game_rules tests OK\n";
    return 0;
}
