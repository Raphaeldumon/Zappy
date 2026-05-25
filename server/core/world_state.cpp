#include "core/world_state.hpp"

#include <stdexcept>

namespace zappy::core {

WorldState::WorldState(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("WorldState dimensions must be positive");
    }
    tiles_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
}

int WorldState::wrap_x(int x) const noexcept {
    int m = x % width_;
    return m < 0 ? m + width_ : m;
}

int WorldState::wrap_y(int y) const noexcept {
    int m = y % height_;
    return m < 0 ? m + height_ : m;
}

std::size_t WorldState::index(int x, int y) const noexcept {
    return static_cast<std::size_t>(wrap_y(y)) * static_cast<std::size_t>(width_) +
           static_cast<std::size_t>(wrap_x(x));
}

Tile& WorldState::at(int x, int y) noexcept {
    return tiles_[index(x, y)];
}

const Tile& WorldState::at(int x, int y) const noexcept {
    return tiles_[index(x, y)];
}

Player& WorldState::add_player(TeamId team, int x, int y, Orientation facing) {
    PlayerId id = next_player_id_++;
    Player p{};
    p.id = id;
    p.team = team;
    p.x = wrap_x(x);
    p.y = wrap_y(y);
    p.orientation = facing;
    auto [it, _] = players_.emplace(id, p);
    return it->second;
}

Player* WorldState::find_player(PlayerId id) noexcept {
    auto it = players_.find(id);
    return it == players_.end() ? nullptr : &it->second;
}

} // namespace zappy::core
