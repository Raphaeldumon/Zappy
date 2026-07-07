#pragma once

#include "core/types.hpp"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zappy::core
{

// Authoritative game world. Owns the map (toroidal grid), players, eggs and teams.
//
// Coordinates wrap on both axes (the map is a torus). Use wrap_x()/wrap_y() or
// at() which already normalize, so callers can pass any signed coordinate.
//
// This class is pure logic: no networking, no time. The runtime drives it; the
// pybind11 simulator reuses it as-is.
class WorldState
{
  public:
    WorldState(int width, int height);

    [[nodiscard]] int width() const noexcept
    {
        return width_;
    }
    [[nodiscard]] int height() const noexcept
    {
        return height_;
    }

    // Normalize a (possibly out-of-range / negative) coordinate onto the torus.
    [[nodiscard]] int wrap_x(int x) const noexcept;
    [[nodiscard]] int wrap_y(int y) const noexcept;

    // Tile access with automatic toroidal wrap.
    [[nodiscard]] Tile &at(int x, int y) noexcept;
    [[nodiscard]] const Tile &at(int x, int y) const noexcept;

    // --- Teams ------------------------------------------------------------------
    void register_team(TeamId id, std::string name, int initial_slots);
    [[nodiscard]] int team_slots(TeamId id) const noexcept;
    void consume_team_slot(TeamId id);
    void restore_team_slot(TeamId id);
    [[nodiscard]] const std::vector<Team> &teams() const noexcept
    {
        return teams_;
    }
    [[nodiscard]] TeamId find_team_by_name(std::string_view name) const;
    [[nodiscard]] std::string_view team_name(TeamId id) const noexcept;

    // --- Players ----------------------------------------------------------------
    Player &add_player(TeamId team, int x, int y, Orientation facing);
    [[nodiscard]] Player *find_player(PlayerId id) noexcept;
    [[nodiscard]] const Player *find_player(PlayerId id) const noexcept;
    [[nodiscard]] std::size_t player_count() const noexcept
    {
        return players_.size();
    }
    [[nodiscard]] const std::unordered_map<PlayerId, Player> &players() const noexcept
    {
        return players_;
    }
    void remove_player(PlayerId id);

    // Returns all player IDs on the given (already wrapped) tile.
    [[nodiscard]] std::vector<PlayerId> players_at(int x, int y) const;

    // Movement
    void move_forward(PlayerId id);
    void turn_right(PlayerId id);
    void turn_left(PlayerId id);

    // Object interaction — returns true on success
    bool take_object(PlayerId id, int resource_index);
    bool set_object(PlayerId id, int resource_index, std::uint64_t now_tick = 0);

    // Ejection — returns {victim_id, K} list (K = direction in victim's frame 1..8)
    // Also marks eggs on the ejector's original tile as hatched (destroyed).
    struct EjectResult
    {
        PlayerId victim;
        int k;
        EggId egg_destroyed{0};
    };
    std::vector<EjectResult> eject(PlayerId ejector_id);

    // Vision cone — tiles in protocol order (row 0 = self, row k: 2k+1 tiles).
    // Level L player sees rows 0..L; total (L+1)^2 tiles.
    struct LookTile
    {
        ResourceSet resources{};
        int player_count{0};
    };
    [[nodiscard]] std::vector<LookTile> look(PlayerId id) const;

    // --- Eggs -------------------------------------------------------------------
    EggId add_egg(PlayerId layer, TeamId team, int x, int y);
    bool hatch_egg(EggId id);
    void remove_egg(EggId id);
    [[nodiscard]] Egg *find_egg(EggId id) noexcept;
    [[nodiscard]] const std::vector<Egg> &eggs() const noexcept
    {
        return eggs_;
    }

    // --- Resources --------------------------------------------------------------
    // Respawn: add resources to meet density targets (called every 20 ticks).
    // Returns the coordinates of the tiles actually modified, so the caller can
    // emit `bct` only for those (the subject forbids re-pushing the whole map).
    std::vector<std::pair<int, int>> respawn_resources(std::uint64_t now_tick = 0);

    // Remove food that has stayed on the ground past its lifetime. Returns the
    // modified tile coordinates for GUI `bct` updates.
    std::vector<std::pair<int, int>> expire_food(std::uint64_t now_tick);

    // --- Win condition ----------------------------------------------------------
    [[nodiscard]] std::optional<TeamId> check_win() const;

  private:
    [[nodiscard]] std::size_t index(int x, int y) const noexcept;

    int width_;
    int height_;
    std::vector<Tile> tiles_;
    std::unordered_map<PlayerId, Player> players_;
    PlayerId next_player_id_{1};
    std::vector<Egg> eggs_;
    EggId next_egg_id_{1};
    std::vector<Team> teams_;
    std::mt19937 rng_;
};

} // namespace zappy::core
