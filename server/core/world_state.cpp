#include "core/world_state.hpp"
#include "core/game_rules.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>

namespace zappy::core
{

// ---------------------------------------------------------------------------
// Orientation helpers (file-local)
// ---------------------------------------------------------------------------

static std::pair<int, int> facing_delta(Orientation o) noexcept
{
    // North = -Y, East = +X, South = +Y, West = -X
    switch (o)
    {
    case Orientation::North:
        return {0, -1};
    case Orientation::East:
        return {1, 0};
    case Orientation::South:
        return {0, 1};
    case Orientation::West:
        return {-1, 0};
    }
    return {0, 0};
}

static std::pair<int, int> right_delta(Orientation o) noexcept
{
    switch (o)
    {
    case Orientation::North:
        return {1, 0}; // East
    case Orientation::East:
        return {0, 1}; // South
    case Orientation::South:
        return {-1, 0}; // West
    case Orientation::West:
        return {0, -1}; // North
    }
    return {0, 0};
}

static Orientation turn_cw(Orientation o) noexcept
{
    switch (o)
    {
    case Orientation::North:
        return Orientation::East;
    case Orientation::East:
        return Orientation::South;
    case Orientation::South:
        return Orientation::West;
    case Orientation::West:
        return Orientation::North;
    }
    return Orientation::North;
}

static Orientation turn_ccw(Orientation o) noexcept
{
    switch (o)
    {
    case Orientation::North:
        return Orientation::West;
    case Orientation::West:
        return Orientation::South;
    case Orientation::South:
        return Orientation::East;
    case Orientation::East:
        return Orientation::North;
    }
    return Orientation::North;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

WorldState::WorldState(int width, int height) : width_(width), height_(height), rng_(std::random_device{}())
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("WorldState dimensions must be positive");
    tiles_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
}

// ---------------------------------------------------------------------------
// Coordinate helpers
// ---------------------------------------------------------------------------

int WorldState::wrap_x(int x) const noexcept
{
    int m = x % width_;
    return m < 0 ? m + width_ : m;
}

int WorldState::wrap_y(int y) const noexcept
{
    int m = y % height_;
    return m < 0 ? m + height_ : m;
}

std::size_t WorldState::index(int x, int y) const noexcept
{
    return static_cast<std::size_t>(wrap_y(y)) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(wrap_x(x));
}

Tile &WorldState::at(int x, int y) noexcept
{
    return tiles_[index(x, y)];
}
const Tile &WorldState::at(int x, int y) const noexcept
{
    return tiles_[index(x, y)];
}

// ---------------------------------------------------------------------------
// Teams
// ---------------------------------------------------------------------------

void WorldState::register_team(TeamId id, std::string name, int initial_slots)
{
    teams_.push_back(Team{id, std::move(name), initial_slots});
}

int WorldState::team_slots(TeamId id) const noexcept
{
    for (const auto &t : teams_)
        if (t.id == id)
            return t.slots_available;
    return 0;
}

void WorldState::consume_team_slot(TeamId id)
{
    for (auto &t : teams_)
        if (t.id == id)
        {
            --t.slots_available;
            return;
        }
}

void WorldState::restore_team_slot(TeamId id)
{
    for (auto &t : teams_)
        if (t.id == id)
        {
            ++t.slots_available;
            return;
        }
}

TeamId WorldState::find_team_by_name(std::string_view name) const
{
    for (const auto &t : teams_)
        if (t.name == name)
            return t.id;
    throw std::invalid_argument(std::string("Unknown team: ") + std::string(name));
}

std::string_view WorldState::team_name(TeamId id) const noexcept
{
    for (const auto &t : teams_)
        if (t.id == id)
            return t.name;
    return "";
}

// ---------------------------------------------------------------------------
// Players
// ---------------------------------------------------------------------------

Player &WorldState::add_player(TeamId team, int x, int y, Orientation facing)
{
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

Player *WorldState::find_player(PlayerId id) noexcept
{
    auto it = players_.find(id);
    return it == players_.end() ? nullptr : &it->second;
}

const Player *WorldState::find_player(PlayerId id) const noexcept
{
    auto it = players_.find(id);
    return it == players_.end() ? nullptr : &it->second;
}

void WorldState::remove_player(PlayerId id)
{
    players_.erase(id);
}

std::vector<PlayerId> WorldState::players_at(int x, int y) const
{
    int wx = wrap_x(x), wy = wrap_y(y);
    std::vector<PlayerId> result;
    for (const auto &[pid, p] : players_)
        if (p.alive && p.x == wx && p.y == wy)
            result.push_back(pid);
    return result;
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------

void WorldState::move_forward(PlayerId id)
{
    auto *p = find_player(id);
    if (!p || !p->alive)
        return;
    auto [dx, dy] = facing_delta(p->orientation);
    p->x = wrap_x(p->x + dx);
    p->y = wrap_y(p->y + dy);
}

void WorldState::turn_right(PlayerId id)
{
    auto *p = find_player(id);
    if (!p || !p->alive)
        return;
    p->orientation = turn_cw(p->orientation);
}

void WorldState::turn_left(PlayerId id)
{
    auto *p = find_player(id);
    if (!p || !p->alive)
        return;
    p->orientation = turn_ccw(p->orientation);
}

// ---------------------------------------------------------------------------
// Object interaction
// ---------------------------------------------------------------------------

bool WorldState::take_object(PlayerId id, int resource_index)
{
    auto *p = find_player(id);
    if (!p || !p->alive)
        return false;
    if (resource_index < 0 || resource_index >= RESOURCE_COUNT)
        return false;
    const auto resource = static_cast<std::size_t>(resource_index);
    auto &tile = at(p->x, p->y);
    if (tile.resources[resource] <= 0)
        return false;
    --tile.resources[resource];
    ++p->inventory[resource];
    return true;
}

bool WorldState::set_object(PlayerId id, int resource_index)
{
    auto *p = find_player(id);
    if (!p || !p->alive)
        return false;
    if (resource_index < 0 || resource_index >= RESOURCE_COUNT)
        return false;
    const auto resource = static_cast<std::size_t>(resource_index);
    if (p->inventory[resource] <= 0)
        return false;
    --p->inventory[resource];
    ++at(p->x, p->y).resources[resource];
    return true;
}

// ---------------------------------------------------------------------------
// Ejection
// ---------------------------------------------------------------------------

// Compute the direction K (1..8) in the victim's reference frame
// that corresponds to the push vector (dx, dy) in world coordinates.
static int eject_k_for_victim(std::pair<int, int> push_delta, Orientation victim_orient)
{
    // The victim arrives from the opposite direction of the push.
    // K describes where the sound/push came from in victim's frame.
    // We compute the direction of (-push_delta) in victim's frame.
    int dx = -push_delta.first;
    int dy = -push_delta.second;
    auto [fw_x, fw_y] = facing_delta(victim_orient);
    auto [rw_x, rw_y] = right_delta(victim_orient);
    int fwd = dx * fw_x + dy * fw_y;
    int rgt = dx * rw_x + dy * rw_y;

    if (fwd > 0 && rgt == 0)
        return 1;
    if (fwd > 0 && rgt > 0 && fwd >= rgt)
        return 2;
    if (fwd == 0 && rgt > 0)
        return 3;
    if (fwd < 0 && rgt > 0 && rgt >= -fwd)
        return 4;
    if (fwd < 0 && rgt == 0)
        return 5;
    if (fwd < 0 && rgt < 0 && -fwd >= -rgt)
        return 6;
    if (fwd == 0 && rgt < 0)
        return 7;
    if (fwd > 0 && rgt < 0 && fwd >= -rgt)
        return 8;
    // diagonal tie-breaks
    if (fwd > 0 && rgt > 0)
        return 2;
    if (fwd < 0 && rgt > 0)
        return 4;
    if (fwd < 0 && rgt < 0)
        return 6;
    if (fwd > 0 && rgt < 0)
        return 8;
    return 1; // fallback
}

std::vector<WorldState::EjectResult> WorldState::eject(PlayerId ejector_id)
{
    auto *ejector = find_player(ejector_id);
    if (!ejector || !ejector->alive)
        return {};

    int ex = ejector->x, ey = ejector->y;
    auto delta = facing_delta(ejector->orientation);
    std::vector<EjectResult> results;

    // Push players
    for (auto &[pid, p] : players_)
    {
        if (pid == ejector_id || !p.alive)
            continue;
        if (p.x != ex || p.y != ey)
            continue;
        int k = eject_k_for_victim(delta, p.orientation);
        p.x = wrap_x(p.x + delta.first);
        p.y = wrap_y(p.y + delta.second);
        results.push_back({pid, k, 0});
    }

    // Destroy eggs on ejector's tile
    for (auto &egg : eggs_)
    {
        if (!egg.hatched && egg.x == ex && egg.y == ey)
        {
            egg.hatched = true;
            // Attach egg id to result so caller can emit edi
            results.push_back({0, 0, egg.id});
        }
    }

    return results;
}

// ---------------------------------------------------------------------------
// Vision (Look)
// ---------------------------------------------------------------------------

std::vector<WorldState::LookTile> WorldState::look(PlayerId id) const
{
    const auto *p = find_player(id);
    if (!p || !p->alive)
        return {};

    int level = p->level;
    auto [fw_x, fw_y] = facing_delta(p->orientation);
    auto [rw_x, rw_y] = right_delta(p->orientation);

    std::vector<LookTile> result;
    result.reserve(static_cast<std::size_t>((level + 1) * (level + 1)));

    for (int row = 0; row <= level; ++row)
    {
        for (int offset = -row; offset <= row; ++offset)
        {
            int tx = wrap_x(p->x + row * fw_x + offset * rw_x);
            int ty = wrap_y(p->y + row * fw_y + offset * rw_y);
            LookTile tile;
            tile.resources = at(tx, ty).resources;
            tile.player_count = static_cast<int>(players_at(tx, ty).size());
            result.push_back(tile);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Eggs
// ---------------------------------------------------------------------------

EggId WorldState::add_egg(PlayerId layer, TeamId team, int x, int y)
{
    EggId id = next_egg_id_++;
    eggs_.push_back(Egg{id, layer, team, wrap_x(x), wrap_y(y), false});
    return id;
}

bool WorldState::hatch_egg(EggId id)
{
    for (auto &egg : eggs_)
    {
        if (egg.id == id && !egg.hatched)
        {
            egg.hatched = true;
            return true;
        }
    }
    return false;
}

void WorldState::remove_egg(EggId id)
{
    eggs_.erase(std::remove_if(eggs_.begin(), eggs_.end(), [id](const Egg &e) { return e.id == id; }), eggs_.end());
}

Egg *WorldState::find_egg(EggId id) noexcept
{
    for (auto &egg : eggs_)
        if (egg.id == id)
            return &egg;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

static constexpr double RESOURCE_DENSITIES[RESOURCE_COUNT] = {0.5, 0.3, 0.15, 0.1, 0.1, 0.08, 0.05};

void WorldState::respawn_resources()
{
    int total_tiles = width_ * height_;
    std::uniform_int_distribution<int> tile_dist(0, total_tiles - 1);

    for (std::size_t r = 0; r < static_cast<std::size_t>(RESOURCE_COUNT); ++r)
    {
        int target = std::max(1, static_cast<int>(width_ * height_ * RESOURCE_DENSITIES[r]));
        int current = 0;
        for (const auto &tile : tiles_)
            current += tile.resources[r];
        int to_add = std::max(0, target - current);
        for (int i = 0; i < to_add; ++i)
        {
            int idx = tile_dist(rng_);
            ++tiles_[static_cast<std::size_t>(idx)].resources[r];
        }
    }
}

// ---------------------------------------------------------------------------
// Win condition
// ---------------------------------------------------------------------------

std::optional<TeamId> WorldState::check_win() const
{
    for (const auto &team : teams_)
    {
        int count = 0;
        for (const auto &[pid, p] : players_)
            if (p.alive && p.team == team.id && p.level == MAX_LEVEL)
                ++count;
        if (count >= 6)
            return team.id;
    }
    return std::nullopt;
}

} // namespace zappy::core
