#include "runtime/server.hpp"

#include "core/game_rules.hpp"
#include "protocol/gui_emitter.hpp"
#include "protocol/handshake.hpp"
#include "runtime/limits.hpp"
#include "zappy/protocol/ai_protocol.hpp"

#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace zappy::runtime
{

std::atomic<bool> Server::running_{true};
std::atomic<bool> Server::dump_snapshot_{false};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Server::Server(const ServerArgs &args)
    : args_(args), world_(args.width, args.height), net_(args.port), rng_(std::random_device{}())
{
    net_.on_connect([this](int fd) { on_client_connect(fd); });
    net_.on_line([this](int fd, std::string line) { on_client_line(fd, std::move(line)); });
    net_.on_disconnect([this](int fd) { on_client_disconnect(fd); });
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void Server::run()
{
    start_time_ = std::chrono::steady_clock::now();
    init_world();
    schedule_resource_respawn();

    while (running_.load(std::memory_order_relaxed))
    {
        dump_snapshot_.store(false, std::memory_order_relaxed); // ignored (no bonus)

        scheduler_.advance_to(now_ticks());
        net_.poll_once(ms_until_next_event());
        announce_win_if_over();
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void Server::init_world()
{
    core::TeamId tid = 0;
    for (auto &name : args_.teams)
    {
        world_.register_team(tid, name, args_.clients_per_team);
        ++tid;
    }
    world_.respawn_resources(now_ticks());
}

void Server::schedule_resource_respawn()
{
    scheduler_.schedule(now_ticks() + 20, [this]() {
        for (auto [x, y] : world_.expire_food(now_ticks()))
            net_.broadcast_gui(protocol::GuiEmitter::bct(x, y, world_.at(x, y).resources));

        // Push bct only for the tiles respawn actually changed (subject: avoid
        // re-broadcasting the whole map every cycle).
        for (auto [x, y] : world_.respawn_resources(now_ticks()))
            net_.broadcast_gui(protocol::GuiEmitter::bct(x, y, world_.at(x, y).resources));
        schedule_resource_respawn();
    });
}

void Server::schedule_food_consumption(core::PlayerId id)
{
    auto ev_id = scheduler_.schedule(now_ticks() + static_cast<core::EventScheduler::Tick>(core::LIFE_UNITS_PER_FOOD),
                                     [this, id]() {
                                         auto *p = world_.find_player(id);
                                         if (!p || !p->alive)
                                             return;
                                         if (p->inventory[0] > 0)
                                         {
                                             p->inventory[0]--;
                                             schedule_food_consumption(id);
                                         }
                                         else
                                         {
                                             kill_player(id);
                                         }
                                     });
    if (auto *p = world_.find_player(id))
        p->food_event_id = ev_id;
}

// ---------------------------------------------------------------------------
// Network callbacks
// ---------------------------------------------------------------------------

void Server::on_client_connect(int fd)
{
    // Guard the fd budget: refuse once we're at the hard ceiling. The fd was
    // just accepted, so clients() already counts it — close it straight back.
    if (static_cast<int>(net_.clients().size()) > MAX_TOTAL_CLIENTS)
    {
        net_.close_client(fd);
        return;
    }
    net_.send_to(fd, protocol::ai::WELCOME);
}

void Server::on_client_line(int fd, std::string line)
{
    auto *client = net_.find_client(fd);
    if (!client)
        return;

    switch (client->state)
    {
    case net::ClientState::Handshake:
        handle_handshake_line(fd, line);
        break;
    case net::ClientState::AI:
        enqueue_ai_command(fd, std::move(line));
        break;
    case net::ClientState::GUI:
        handle_gui_request(fd, line);
        break;
    case net::ClientState::Dead:
        break;
    }
}

void Server::on_client_disconnect(int fd)
{
    gui_fds_.erase(fd);

    auto it = fd_to_player_.find(fd);
    if (it == fd_to_player_.end())
        return;

    core::PlayerId pid = it->second;
    auto *p = world_.find_player(pid);
    if (p)
    {
        scheduler_.cancel(p->food_event_id);
        scheduler_.cancel(p->cmd_event_id);
        p->alive = false;
    }
    player_to_fd_.erase(pid);
    fd_to_player_.erase(fd);

    // Restore team slot so another client can connect
    if (p)
        world_.restore_team_slot(p->team);

    world_.remove_player(pid);
}

// ---------------------------------------------------------------------------
// Handshake
// ---------------------------------------------------------------------------

void Server::handle_handshake_line(int fd, std::string_view line)
{
    std::string team_name_out;
    auto result = protocol::handle_handshake(line, world_.teams(), team_name_out);

    if (result == protocol::HandshakeResult::GUI)
    {
        complete_gui_handshake(fd);
    }
    else if (result == protocol::HandshakeResult::AI)
    {
        core::TeamId tid = world_.find_team_by_name(team_name_out);
        complete_ai_handshake(fd, tid, team_name_out);
    }
    else
    {
        // Unknown team — reject
        net_.close_client(fd);
    }
}

void Server::complete_ai_handshake(int fd, core::TeamId team_id, std::string_view team_name)
{
    if (world_.team_slots(team_id) <= 0)
    {
        net_.close_client(fd);
        return;
    }
    world_.consume_team_slot(team_id);

    // Spawn: use egg if available, else random tile
    int sx = static_cast<int>(rng_() % static_cast<unsigned>(world_.width()));
    int sy = static_cast<int>(rng_() % static_cast<unsigned>(world_.height()));
    core::EggId used_egg{0};

    for (auto &egg : world_.eggs())
    {
        if (!egg.hatched && egg.team == team_id)
        {
            sx = egg.x;
            sy = egg.y;
            used_egg = egg.id;
            world_.hatch_egg(egg.id);
            break;
        }
    }

    auto orient = static_cast<core::Orientation>(1 + (rng_() % 4)); // 1..4

    auto &p = world_.add_player(team_id, sx, sy, orient);
    p.inventory[0] = core::STARTING_FOOD;

    fd_to_player_[fd] = p.id;
    player_to_fd_[p.id] = fd;

    auto *client = net_.find_client(fd);
    if (client)
    {
        client->state = net::ClientState::AI;
        client->player_id = p.id;
    }

    net_.send_to(fd, std::to_string(world_.team_slots(team_id)));
    net_.send_to(fd, std::to_string(world_.width()) + " " + std::to_string(world_.height()));

    if (used_egg)
        net_.broadcast_gui(protocol::GuiEmitter::ebo(used_egg));
    net_.broadcast_gui(protocol::GuiEmitter::pnw(p.id, p.x, p.y, p.orientation, p.level, team_name));

    schedule_food_consumption(p.id);
}

void Server::complete_gui_handshake(int fd)
{
    // Cap concurrent GUIs: each gets full snapshots, so an unbounded count is a
    // cheap DoS surface. Reject beyond the limit.
    if (static_cast<int>(gui_fds_.size()) >= MAX_GUI_CLIENTS)
    {
        net_.close_client(fd);
        return;
    }
    auto *client = net_.find_client(fd);
    if (client)
        client->state = net::ClientState::GUI;
    gui_fds_.insert(fd);
    send_gui_snapshot(fd);
}

void Server::send_gui_snapshot(int fd)
{
    net_.send_to(fd, protocol::GuiEmitter::msz(world_.width(), world_.height()));
    net_.send_to(fd, protocol::GuiEmitter::sgt(args_.frequency));

    for (auto &t : world_.teams())
        net_.send_to(fd, protocol::GuiEmitter::tna(t.name));

    for (int y = 0; y < world_.height(); ++y)
        for (int x = 0; x < world_.width(); ++x)
            net_.send_to(fd, protocol::GuiEmitter::bct(x, y, world_.at(x, y).resources));

    for (auto &[pid, player] : world_.players())
    {
        if (!player.alive)
            continue;
        net_.send_to(fd, protocol::GuiEmitter::pnw(pid, player.x, player.y, player.orientation, player.level,
                                                   world_.team_name(player.team)));
    }

    for (auto &egg : world_.eggs())
    {
        if (!egg.hatched)
            net_.send_to(fd, protocol::GuiEmitter::enw(egg.id, egg.layer, egg.x, egg.y));
    }
}

// ---------------------------------------------------------------------------
// Player death
// ---------------------------------------------------------------------------

void Server::kill_player(core::PlayerId id)
{
    auto *p = world_.find_player(id);
    if (!p || !p->alive)
        return;
    p->alive = false;

    if (auto fd_it = player_to_fd_.find(id); fd_it != player_to_fd_.end())
    {
        net_.send_to(fd_it->second, protocol::ai::DEAD);
        net_.close_client(fd_it->second);
    }
    net_.broadcast_gui(protocol::GuiEmitter::pdi(id));
}

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

// Resolve the live player behind a socket, or null. Unlike fd_to_player_[fd],
// this never inserts a phantom entry on an unknown fd.
core::Player *Server::player_for(int fd) noexcept
{
    auto it = fd_to_player_.find(fd);
    if (it == fd_to_player_.end())
        return nullptr;
    return world_.find_player(it->second);
}

void Server::send_ok(int fd)
{
    net_.send_to(fd, protocol::ai::OK);
}

void Server::send_ko(int fd)
{
    net_.send_to(fd, protocol::ai::KO);
}

void Server::broadcast_player_pos(core::PlayerId pid)
{
    if (auto *p = world_.find_player(pid))
        net_.broadcast_gui(protocol::GuiEmitter::ppo(pid, p->x, p->y, p->orientation));
}

bool Server::announce_win_if_over()
{
    auto winner = world_.check_win();
    if (!winner)
        return false;
    net_.broadcast_gui(protocol::GuiEmitter::seg(world_.team_name(*winner)));
    running_ = false;
    return true;
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

core::EventScheduler::Tick Server::now_ticks() const noexcept
{
    using namespace std::chrono;
    auto ns = duration_cast<nanoseconds>(steady_clock::now() - start_time_).count();
    return static_cast<core::EventScheduler::Tick>(static_cast<double>(ns) * args_.frequency / 1'000'000'000.0);
}

int Server::ms_until_next_event() const noexcept
{
    if (scheduler_.empty())
        return 20;
    auto next = scheduler_.next_event_tick();
    auto now = now_ticks();
    if (next <= now)
        return 0;
    auto ticks_left = next - now;
    return static_cast<int>(ticks_left * 1000 / static_cast<std::uint64_t>(args_.frequency)) + 1;
}

} // namespace zappy::runtime
