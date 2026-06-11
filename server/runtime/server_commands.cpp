#include "runtime/server.hpp"

#include "core/game_rules.hpp"
#include "protocol/gui_emitter.hpp"
#include "protocol/gui_handler.hpp"
#include "zappy/protocol/ai_protocol.hpp"

#include <string>
#include <vector>

namespace zappy::runtime
{

// ---------------------------------------------------------------------------
// AI command pipeline
// ---------------------------------------------------------------------------

void Server::enqueue_ai_command(int fd, std::string line)
{
    auto *client = net_.find_client(fd);
    if (!client)
        return;

    if (static_cast<int>(client->command_queue.size()) >= protocol::ai::MAX_COMMAND_QUEUE)
        return; // silent drop

    client->command_queue.push_back(std::move(line));

    if (!client->has_pending_action)
        execute_next_command(fd);
}

void Server::execute_next_command(int fd)
{
    auto *client = net_.find_client(fd);
    if (!client || client->command_queue.empty())
        return;

    auto it = fd_to_player_.find(fd);
    if (it == fd_to_player_.end())
        return;
    auto *player = world_.find_player(it->second);
    if (!player || !player->alive)
        return;
    if (player->incanting)
        return; // frozen until incantation completes

    std::string cmd_str = client->command_queue.front();
    client->command_queue.pop_front();

    auto parsed = protocol::parse_ai_command(cmd_str);
    if (!parsed)
    {
        net_.send_to(fd, std::string(protocol::ai::KO));
        execute_next_command(fd); // bad commands have no time cost
        return;
    }

    // ConnectNbr: instant, no scheduler event
    if (parsed->cmd == protocol::ai::Command::ConnectNbr)
    {
        cmd_connect_nbr(fd);
        execute_next_command(fd);
        return;
    }

    client->has_pending_action = true;
    int cost = protocol::ai::time_cost(parsed->cmd);
    auto ev_id =
        scheduler_.schedule(now_ticks() + static_cast<core::EventScheduler::Tick>(cost), [this, fd, p = *parsed]() {
            dispatch_command(fd, p);
            auto *c = net_.find_client(fd);
            if (c)
            {
                c->has_pending_action = false;
                execute_next_command(fd);
            }
        });

    auto pid_it = fd_to_player_.find(fd);
    if (pid_it != fd_to_player_.end())
    {
        if (auto *pp = world_.find_player(pid_it->second))
            pp->cmd_event_id = ev_id;
    }
}

void Server::dispatch_command(int fd, const protocol::ParsedCommand &cmd)
{
    auto it = fd_to_player_.find(fd);
    if (it == fd_to_player_.end())
        return;
    auto *p = world_.find_player(it->second);
    if (!p || !p->alive)
        return;

    using Cmd = protocol::ai::Command;
    switch (cmd.cmd)
    {
    case Cmd::Forward:
        cmd_forward(fd);
        break;
    case Cmd::Right:
        cmd_right(fd);
        break;
    case Cmd::Left:
        cmd_left(fd);
        break;
    case Cmd::Look:
        cmd_look(fd);
        break;
    case Cmd::Inventory:
        cmd_inventory(fd);
        break;
    case Cmd::Broadcast:
        cmd_broadcast(fd, cmd.arg);
        break;
    case Cmd::Fork:
        cmd_fork(fd);
        break;
    case Cmd::Eject:
        cmd_eject(fd);
        break;
    case Cmd::Take:
        cmd_take(fd, cmd.resource_index);
        break;
    case Cmd::Set:
        cmd_set(fd, cmd.resource_index);
        break;
    case Cmd::Incantation:
        cmd_incantation(fd);
        break;
    case Cmd::ConnectNbr:
        cmd_connect_nbr(fd);
        break; // handled before schedule
    case Cmd::Count_:
        break;
    }
}

// ---------------------------------------------------------------------------
// AI command handlers
// ---------------------------------------------------------------------------

void Server::cmd_forward(int fd)
{
    auto pid = fd_to_player_[fd];
    world_.move_forward(pid);
    auto *p = world_.find_player(pid);
    net_.send_to(fd, std::string(protocol::ai::OK));
    if (p)
        net_.broadcast_gui(protocol::GuiEmitter::ppo(pid, p->x, p->y, p->orientation));
}

void Server::cmd_right(int fd)
{
    auto pid = fd_to_player_[fd];
    world_.turn_right(pid);
    auto *p = world_.find_player(pid);
    net_.send_to(fd, std::string(protocol::ai::OK));
    if (p)
        net_.broadcast_gui(protocol::GuiEmitter::ppo(pid, p->x, p->y, p->orientation));
}

void Server::cmd_left(int fd)
{
    auto pid = fd_to_player_[fd];
    world_.turn_left(pid);
    auto *p = world_.find_player(pid);
    net_.send_to(fd, std::string(protocol::ai::OK));
    if (p)
        net_.broadcast_gui(protocol::GuiEmitter::ppo(pid, p->x, p->y, p->orientation));
}

void Server::cmd_look(int fd)
{
    auto pid = fd_to_player_[fd];
    auto tiles = world_.look(pid);
    net_.send_to(fd, protocol::fmt_look(tiles));
}

void Server::cmd_inventory(int fd)
{
    auto pid = fd_to_player_[fd];
    auto *p = world_.find_player(pid);
    if (!p)
        return;
    net_.send_to(fd, protocol::fmt_inventory(p->inventory));
    net_.broadcast_gui(protocol::GuiEmitter::pin(pid, p->x, p->y, p->inventory));
}

void Server::cmd_broadcast(int fd, std::string text)
{
    auto pid = fd_to_player_[fd];
    net_.send_to(fd, std::string(protocol::ai::OK));
    broadcast_message_to_ai(pid, text);
}

void Server::cmd_connect_nbr(int fd)
{
    auto pid = fd_to_player_[fd];
    auto *p = world_.find_player(pid);
    if (!p)
        return;
    int slots = world_.team_slots(p->team);
    net_.send_to(fd, protocol::fmt_connect_nbr(slots));
}

void Server::cmd_fork(int fd)
{
    auto pid = fd_to_player_[fd];
    auto *p = world_.find_player(pid);
    if (!p)
        return;

    core::EggId egg_id = world_.add_egg(p->id, p->team, p->x, p->y);
    world_.restore_team_slot(p->team); // egg adds one pending slot

    net_.send_to(fd, std::string(protocol::ai::OK));
    net_.broadcast_gui(protocol::GuiEmitter::pfk(pid));
    net_.broadcast_gui(protocol::GuiEmitter::enw(egg_id, pid, p->x, p->y));
}

void Server::cmd_eject(int fd)
{
    auto pid = fd_to_player_[fd];
    auto results = world_.eject(pid);

    for (auto &r : results)
    {
        if (r.victim != 0)
        {
            // Notify victim
            if (auto vfd_it = player_to_fd_.find(r.victim); vfd_it != player_to_fd_.end())
            {
                net_.send_to(vfd_it->second, "eject: " + std::to_string(r.k));
            }
            // Update GUIs on victim's new position
            auto *v = world_.find_player(r.victim);
            if (v)
                net_.broadcast_gui(protocol::GuiEmitter::ppo(r.victim, v->x, v->y, v->orientation));
        }
        if (r.egg_destroyed != 0)
            net_.broadcast_gui(protocol::GuiEmitter::edi(r.egg_destroyed));
    }

    net_.broadcast_gui(protocol::GuiEmitter::pex(pid));
    net_.send_to(fd, std::string(protocol::ai::OK));
}

void Server::cmd_take(int fd, int resource_index)
{
    auto pid = fd_to_player_[fd];
    auto *p = world_.find_player(pid);
    if (!p)
        return;

    if (world_.take_object(pid, resource_index))
    {
        net_.send_to(fd, std::string(protocol::ai::OK));
        net_.broadcast_gui(protocol::GuiEmitter::pgt(pid, resource_index));
        net_.broadcast_gui(protocol::GuiEmitter::bct(p->x, p->y, world_.at(p->x, p->y).resources));
    }
    else
    {
        net_.send_to(fd, std::string(protocol::ai::KO));
    }
}

void Server::cmd_set(int fd, int resource_index)
{
    auto pid = fd_to_player_[fd];
    auto *p = world_.find_player(pid);
    if (!p)
        return;

    if (world_.set_object(pid, resource_index))
    {
        net_.send_to(fd, std::string(protocol::ai::OK));
        net_.broadcast_gui(protocol::GuiEmitter::pdr(pid, resource_index));
        net_.broadcast_gui(protocol::GuiEmitter::bct(p->x, p->y, world_.at(p->x, p->y).resources));
    }
    else
    {
        net_.send_to(fd, std::string(protocol::ai::KO));
    }
}

void Server::cmd_incantation(int fd)
{
    auto pid = fd_to_player_[fd];
    auto *p = world_.find_player(pid);
    if (!p)
        return;

    int x = p->x, y = p->y, level = p->level;

    if (!core::can_elevate(world_, x, y, level))
    {
        net_.send_to(fd, std::string(protocol::ai::KO));
        return;
    }

    // Gather all same-level players on this tile
    auto pids_on_tile = world_.players_at(x, y);
    std::vector<core::PlayerId> participants;
    for (auto tid : pids_on_tile)
    {
        auto *tp = world_.find_player(tid);
        if (tp && tp->alive && tp->level == level)
            participants.push_back(tid);
    }

    // Freeze all participants
    for (auto part_id : participants)
    {
        if (auto *pp = world_.find_player(part_id))
            pp->incanting = true;
    }

    net_.send_to(fd, "Elevation underway");

    // Notify GUIs
    std::vector<core::PlayerId> part_ids(participants);
    net_.broadcast_gui(protocol::GuiEmitter::pic(x, y, level, part_ids));

    // Schedule completion after 300 ticks
    auto ev_id = scheduler_.schedule(
        now_ticks() +
            static_cast<core::EventScheduler::Tick>(protocol::ai::time_cost(protocol::ai::Command::Incantation)),
        [this, pid, participants, x, y, level]() { cmd_incantation_complete(pid, participants, x, y, level); });

    incantations_[pid] = IncantState{ev_id, participants, x, y, level};
}

void Server::cmd_incantation_complete(core::PlayerId initiator_id, std::vector<core::PlayerId> participants, int x,
                                      int y, int level)
{
    incantations_.erase(initiator_id);

    // Re-check elevation (conditions may have changed)
    if (!core::can_elevate(world_, x, y, level))
    {
        // Failure
        net_.broadcast_gui(protocol::GuiEmitter::pie(x, y, false));
        for (auto part_id : participants)
        {
            auto *pp = world_.find_player(part_id);
            if (!pp || !pp->alive)
                continue;
            pp->incanting = false;
            if (auto fd_it = player_to_fd_.find(part_id); fd_it != player_to_fd_.end())
                net_.send_to(fd_it->second, std::string(protocol::ai::KO));
            execute_next_command(player_to_fd_.count(part_id) ? player_to_fd_.at(part_id) : -1);
        }
        return;
    }

    // Success
    core::consume_elevation_stones(world_, x, y, level);

    for (auto part_id : participants)
    {
        auto *pp = world_.find_player(part_id);
        if (!pp || !pp->alive)
            continue;
        pp->level++;
        pp->incanting = false;
        net_.broadcast_gui(protocol::GuiEmitter::plv(part_id, pp->level));
        if (auto fd_it = player_to_fd_.find(part_id); fd_it != player_to_fd_.end())
        {
            net_.send_to(fd_it->second, "Current level: " + std::to_string(pp->level));
        }
    }

    net_.broadcast_gui(protocol::GuiEmitter::pie(x, y, true));
    net_.broadcast_gui(protocol::GuiEmitter::bct(x, y, world_.at(x, y).resources));

    // Unfreeze and continue executing commands
    for (auto part_id : participants)
    {
        auto *pp = world_.find_player(part_id);
        if (!pp || !pp->alive)
            continue;
        if (auto fd_it = player_to_fd_.find(part_id); fd_it != player_to_fd_.end())
            execute_next_command(fd_it->second);
    }

    // Check win condition immediately after elevation
    if (auto winner = world_.check_win())
    {
        net_.broadcast_gui(protocol::GuiEmitter::seg(world_.team_name(*winner)));
        running_ = false;
    }
}

// ---------------------------------------------------------------------------
// GUI request dispatch
// ---------------------------------------------------------------------------

void Server::handle_gui_request(int fd, std::string_view line)
{
    auto req = protocol::parse_gui_request(line);

    switch (req.type)
    {
    case protocol::GuiRequest::Msz:
        net_.send_to(fd, protocol::GuiEmitter::msz(world_.width(), world_.height()));
        break;

    case protocol::GuiRequest::Mct:
        for (int y = 0; y < world_.height(); ++y)
            for (int x = 0; x < world_.width(); ++x)
                net_.send_to(fd, protocol::GuiEmitter::bct(x, y, world_.at(x, y).resources));
        break;

    case protocol::GuiRequest::Tna:
        for (auto &t : world_.teams())
            net_.send_to(fd, protocol::GuiEmitter::tna(t.name));
        break;

    case protocol::GuiRequest::Bct:
        if (req.x >= 0 && req.y >= 0 && req.x < world_.width() && req.y < world_.height())
        {
            net_.send_to(fd, protocol::GuiEmitter::bct(req.x, req.y, world_.at(req.x, req.y).resources));
        }
        else
        {
            net_.send_to(fd, protocol::GuiEmitter::sbp());
        }
        break;

    case protocol::GuiRequest::Ppo: {
        auto *p = world_.find_player(static_cast<core::PlayerId>(req.n));
        if (p && p->alive)
            net_.send_to(fd, protocol::GuiEmitter::ppo(p->id, p->x, p->y, p->orientation));
        else
            net_.send_to(fd, protocol::GuiEmitter::sbp());
        break;
    }

    case protocol::GuiRequest::Plv: {
        auto *p = world_.find_player(static_cast<core::PlayerId>(req.n));
        if (p && p->alive)
            net_.send_to(fd, protocol::GuiEmitter::plv(p->id, p->level));
        else
            net_.send_to(fd, protocol::GuiEmitter::sbp());
        break;
    }

    case protocol::GuiRequest::Pin: {
        auto *p = world_.find_player(static_cast<core::PlayerId>(req.n));
        if (p && p->alive)
            net_.send_to(fd, protocol::GuiEmitter::pin(p->id, p->x, p->y, p->inventory));
        else
            net_.send_to(fd, protocol::GuiEmitter::sbp());
        break;
    }

    case protocol::GuiRequest::Sgt:
        net_.send_to(fd, protocol::GuiEmitter::sgt(args_.frequency));
        break;

    case protocol::GuiRequest::Sst:
        if (req.t > 0)
        {
            args_.frequency = req.t;
            net_.broadcast_gui(protocol::GuiEmitter::sst(args_.frequency));
        }
        else
        {
            net_.send_to(fd, protocol::GuiEmitter::sbp());
        }
        break;

    case protocol::GuiRequest::Unknown:
        net_.send_to(fd, protocol::GuiEmitter::suc());
        break;

    case protocol::GuiRequest::BadParam:
        net_.send_to(fd, protocol::GuiEmitter::sbp());
        break;
    }
}

// ---------------------------------------------------------------------------
// Broadcast command helper
// ---------------------------------------------------------------------------

void Server::broadcast_message_to_ai(core::PlayerId sender_id, std::string_view text)
{
    auto *sender = world_.find_player(sender_id);
    if (!sender)
        return;

    for (auto &[recv_fd, recv_pid] : fd_to_player_)
    {
        if (recv_pid == sender_id)
            continue;
        auto *receiver = world_.find_player(recv_pid);
        if (!receiver || !receiver->alive)
            continue;

        int k = core::broadcast_direction(sender->x, sender->y, receiver->x, receiver->y, receiver->orientation,
                                          world_.width(), world_.height());

        net_.send_to(recv_fd, "message " + std::to_string(k) + ", " + std::string(text));
    }
    net_.broadcast_gui(protocol::GuiEmitter::pbc(sender_id, text));
}

} // namespace zappy::runtime
