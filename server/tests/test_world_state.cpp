// Skeleton unit tests for WorldState. Migrate to Catch2 once vcpkg is wired (ADR-006).
#include "core/world_state.hpp"

#include <cassert>
#include <iostream>

using namespace zappy::core;

static void test_toroidal_wrap()
{
    WorldState w(10, 8);
    assert(w.wrap_x(0) == 0);
    assert(w.wrap_x(10) == 0); // wraps at width
    assert(w.wrap_x(-1) == 9); // negative wraps to far edge
    assert(w.wrap_x(23) == 3);
    assert(w.wrap_y(-1) == 7);
    assert(w.wrap_y(8) == 0);
    assert(w.wrap_y(17) == 1);
}

static void test_tile_identity_across_wrap()
{
    WorldState w(10, 8);
    w.at(0, 0).resources[0] = 5;
    // (0,0) and (10,8) are the same tile on the torus.
    assert(w.at(10, 8).resources[0] == 5);
    assert(w.at(-10, -8).resources[0] == 5);
}

static void test_add_player()
{
    WorldState w(5, 5);
    assert(w.player_count() == 0);
    Player &p = w.add_player(/*team=*/1, /*x=*/7, /*y=*/-1, Orientation::East);
    assert(w.player_count() == 1);
    assert(p.x == 2); // 7 wrapped onto width 5
    assert(p.y == 4); // -1 wrapped onto height 5
    assert(w.find_player(p.id) != nullptr);
    assert(w.find_player(9999) == nullptr);
}

int main()
{
    test_toroidal_wrap();
    test_tile_identity_across_wrap();
    test_add_player();
    std::cout << "world_state tests OK\n";
    return 0;
}
