#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace zappy::core {

// Authoritative game world. Owns the map (toroidal grid), players and eggs.
//
// Coordinates wrap on both axes (the map is a torus). Use wrap_x()/wrap_y() or
// at() which already normalize, so callers can pass any signed coordinate.
//
// This class is pure logic: no networking, no time. The runtime drives it; the
// pybind11 simulator reuses it as-is.
class WorldState {
public:
    WorldState(int width, int height);

    [[nodiscard]] int width() const noexcept {
        return width_;
    }
    [[nodiscard]] int height() const noexcept {
        return height_;
    }

    // Normalize a (possibly out-of-range / negative) coordinate onto the torus.
    [[nodiscard]] int wrap_x(int x) const noexcept;
    [[nodiscard]] int wrap_y(int y) const noexcept;

    // Tile access with automatic toroidal wrap.
    [[nodiscard]] Tile& at(int x, int y) noexcept;
    [[nodiscard]] const Tile& at(int x, int y) const noexcept;

    // --- Players (skeleton; P1 fleshes these out in S1/S2) -------------------
    Player& add_player(TeamId team, int x, int y, Orientation facing);
    [[nodiscard]] Player* find_player(PlayerId id) noexcept;
    [[nodiscard]] std::size_t player_count() const noexcept {
        return players_.size();
    }

    // TODO(P1): move_forward / turn_left / turn_right / take / set / look(vision cone)
    //           with toroidal wrap. See docs/01_architecture/02_server.md.

private:
    [[nodiscard]] std::size_t index(int x, int y) const noexcept;

    int width_;
    int height_;
    std::vector<Tile> tiles_;
    std::unordered_map<PlayerId, Player> players_;
    PlayerId next_player_id_{1};
};

} // namespace zappy::core
