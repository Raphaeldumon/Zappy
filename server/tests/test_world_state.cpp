// Unit tests for WorldState — the core game logic. Migrate to Catch2 once vcpkg
// is wired (ADR-006).
#include "core/world_state.hpp"

#include "core/game_rules.hpp"

#include <algorithm>
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

// North = -Y, East = +X, South = +Y, West = -X.
static void test_move_forward_each_orientation()
{
    WorldState w(10, 10);
    Player &n = w.add_player(0, 5, 5, Orientation::North);
    w.move_forward(n.id);
    assert(n.x == 5 && n.y == 4);

    Player &e = w.add_player(0, 5, 5, Orientation::East);
    w.move_forward(e.id);
    assert(e.x == 6 && e.y == 5);

    Player &s = w.add_player(0, 5, 5, Orientation::South);
    w.move_forward(s.id);
    assert(s.x == 5 && s.y == 6);

    Player &west = w.add_player(0, 5, 5, Orientation::West);
    w.move_forward(west.id);
    assert(west.x == 4 && west.y == 5);
}

static void test_move_forward_wraps()
{
    WorldState w(4, 4);
    Player &p = w.add_player(0, 0, 0, Orientation::West); // -X off the left edge
    w.move_forward(p.id);
    assert(p.x == 3 && p.y == 0); // wrapped to far right

    Player &q = w.add_player(0, 0, 0, Orientation::North); // -Y off the top
    w.move_forward(q.id);
    assert(q.x == 0 && q.y == 3); // wrapped to bottom
}

static void test_turns_cycle()
{
    WorldState w(5, 5);
    Player &p = w.add_player(0, 2, 2, Orientation::North);

    w.turn_right(p.id);
    assert(p.orientation == Orientation::East);
    w.turn_right(p.id);
    assert(p.orientation == Orientation::South);
    w.turn_right(p.id);
    assert(p.orientation == Orientation::West);
    w.turn_right(p.id);
    assert(p.orientation == Orientation::North); // full circle

    w.turn_left(p.id);
    assert(p.orientation == Orientation::West); // left is the inverse
}

static void test_take_and_set_object()
{
    WorldState w(5, 5);
    Player &p = w.add_player(0, 2, 2, Orientation::North);
    w.at(2, 2).resources[0] = 1; // one food on the tile

    assert(w.take_object(p.id, 0));       // succeeds
    assert(p.inventory[0] == 1);          // moved into inventory
    assert(w.at(2, 2).resources[0] == 0); // gone from the tile

    assert(!w.take_object(p.id, 0)); // tile empty now -> ko

    assert(w.set_object(p.id, 0)); // drop it back
    assert(p.inventory[0] == 0);
    assert(w.at(2, 2).resources[0] == 1);

    assert(!w.set_object(p.id, 0)); // nothing left to drop -> ko
}

static void test_take_set_bad_index()
{
    WorldState w(5, 5);
    Player &p = w.add_player(0, 0, 0, Orientation::North);
    assert(!w.take_object(p.id, -1));
    assert(!w.take_object(p.id, RESOURCE_COUNT));
    assert(!w.set_object(p.id, 99));
}

static void test_team_slots()
{
    WorldState w(5, 5);
    w.register_team(0, "red", 2);
    assert(w.team_slots(0) == 2);
    w.consume_team_slot(0);
    assert(w.team_slots(0) == 1);
    w.consume_team_slot(0);
    assert(w.team_slots(0) == 0);
    w.restore_team_slot(0); // Fork / disconnect gives one back
    assert(w.team_slots(0) == 1);
    assert(w.find_team_by_name("red") == 0);
}

static void test_eject_pushes_and_destroys_eggs()
{
    WorldState w(10, 10);
    // Ejector faces East; victim shares the tile.
    Player &ejector = w.add_player(0, 5, 5, Orientation::East);
    Player &victim = w.add_player(1, 5, 5, Orientation::North);
    EggId egg = w.add_egg(ejector.id, 0, 5, 5);

    auto results = w.eject(ejector.id);

    // Victim pushed one tile East (ejector's facing).
    assert(victim.x == 6 && victim.y == 5);

    bool victim_reported = false;
    bool egg_reported = false;
    for (const auto &r : results)
    {
        if (r.victim == victim.id)
            victim_reported = true;
        if (r.egg_destroyed == egg)
            egg_reported = true;
    }
    assert(victim_reported);
    assert(egg_reported);
    assert(w.find_egg(egg)->hatched); // destroyed = marked hatched
}

static void test_look_tile_count_scales_with_level()
{
    WorldState w(20, 20);
    Player &p = w.add_player(0, 10, 10, Orientation::North);

    // Level 1: tiles = 1 (self) + 3 (front row) = 4 = (level+1)^2.
    auto v1 = w.look(p.id);
    assert(v1.size() == 4);

    p.level = 2; // (2+1)^2 = 9
    auto v2 = w.look(p.id);
    assert(v2.size() == 9);

    // Tile 0 is the player's own tile and must report the player.
    assert(v1[0].player_count >= 1);
}

static void test_look_sees_resources_ahead()
{
    WorldState w(20, 20);
    Player &p = w.add_player(0, 10, 10, Orientation::North); // North = -Y
    w.at(10, 9).resources[3] = 2;                            // one tile in front

    auto v = w.look(p.id);
    // Row 1 spans indices 1..3 (left, center, right); the tile directly ahead
    // is the centre of that row = index 2.
    assert(v.size() >= 4);
    assert(v[2].resources[3] == 2);
}

static void test_respawn_reports_changed_tiles_only()
{
    WorldState w(10, 10);

    // First respawn fills an empty map toward density targets: it changes some
    // tiles but never the whole map, and each reported coord is in range + unique.
    auto changed = w.respawn_resources();
    assert(!changed.empty());
    assert(changed.size() < static_cast<std::size_t>(10 * 10)); // not the whole map

    std::vector<int> seen;
    for (auto [x, y] : changed)
    {
        assert(x >= 0 && x < 10 && y >= 0 && y < 10);
        int idx = y * 10 + x;
        assert(std::find(seen.begin(), seen.end(), idx) == seen.end()); // no dupes
        seen.push_back(idx);
    }

    // At/over density, a second respawn has little or nothing to add.
    auto again = w.respawn_resources();
    assert(again.size() <= changed.size());
}

static void test_food_expires_after_ground_lifetime()
{
    WorldState w(1, 1);

    auto changed = w.respawn_resources(/*now_tick=*/10);
    assert(changed.size() == 1);
    assert(w.at(0, 0).resources[0] == 1);

    assert(w.expire_food(10 + FOOD_GROUND_LIFETIME_TICKS - 1).empty());
    assert(w.at(0, 0).resources[0] == 1);

    auto expired = w.expire_food(10 + FOOD_GROUND_LIFETIME_TICKS);
    assert(expired.size() == 1);
    assert(expired[0].first == 0 && expired[0].second == 0);
    assert(w.at(0, 0).resources[0] == 0);
}

static void test_taken_food_no_longer_expires_on_tile()
{
    WorldState w(1, 1);
    Player &p = w.add_player(0, 0, 0, Orientation::North);

    w.respawn_resources(/*now_tick=*/50);
    assert(w.take_object(p.id, 0));
    assert(p.inventory[0] == 1);
    assert(w.at(0, 0).resources[0] == 0);

    assert(w.expire_food(50 + FOOD_GROUND_LIFETIME_TICKS).empty());
    assert(p.inventory[0] == 1);
}

static void test_meteor_strike_clears_tile_and_blocks_respawn()
{
    WorldState w(1, 1);
    Player &p = w.add_player(0, 0, 0, Orientation::North);
    EggId egg = w.add_egg(p.id, 0, 0, 0);
    w.at(0, 0).resources[0] = 4;
    w.at(0, 0).resources[1] = 2;
    w.at(0, 0).food_expirations = {10, 20};

    auto result = w.meteor_strike(0, 0);

    assert(result.players_hit.size() == 1 && result.players_hit[0] == p.id);
    assert(result.eggs_destroyed.size() == 1 && result.eggs_destroyed[0] == egg);
    for (int q : w.at(0, 0).resources)
        assert(q == 0);
    assert(w.at(0, 0).food_expirations.empty());
    assert(w.find_egg(egg)->hatched);
    assert(w.respawn_resources(/*now_tick=*/100).empty());
    for (int q : w.at(0, 0).resources)
        assert(q == 0);
}

static void test_check_win()
{
    WorldState w(10, 10);
    w.register_team(0, "red", 10);
    assert(!w.check_win().has_value());

    // Six max-level players on the same team triggers the win.
    for (int i = 0; i < 6; ++i)
    {
        Player &p = w.add_player(0, i, 0, Orientation::North);
        p.level = MAX_LEVEL;
    }
    auto winner = w.check_win();
    assert(winner.has_value() && *winner == 0);
}

static void test_check_win_needs_six()
{
    WorldState w(10, 10);
    w.register_team(0, "red", 10);
    for (int i = 0; i < 5; ++i) // only five
    {
        Player &p = w.add_player(0, i, 0, Orientation::North);
        p.level = MAX_LEVEL;
    }
    assert(!w.check_win().has_value());
}

int main()
{
    test_toroidal_wrap();
    test_tile_identity_across_wrap();
    test_add_player();
    test_move_forward_each_orientation();
    test_move_forward_wraps();
    test_turns_cycle();
    test_take_and_set_object();
    test_take_set_bad_index();
    test_team_slots();
    test_eject_pushes_and_destroys_eggs();
    test_look_tile_count_scales_with_level();
    test_look_sees_resources_ahead();
    test_respawn_reports_changed_tiles_only();
    test_food_expires_after_ground_lifetime();
    test_taken_food_no_longer_expires_on_tile();
    test_meteor_strike_clears_tile_and_blocks_respawn();
    test_check_win();
    test_check_win_needs_six();
    std::cout << "world_state tests OK\n";
    return 0;
}
