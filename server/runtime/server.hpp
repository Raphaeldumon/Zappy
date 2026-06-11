#pragma once

#include "core/event_scheduler.hpp"
#include "core/world_state.hpp"
#include "net/network_layer.hpp"
#include "protocol/ai_handler.hpp"
#include "runtime/parse_args.hpp"

#include <atomic>
#include <chrono>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace zappy::runtime
{

class Server
{
  public:
    explicit Server(const ServerArgs &args);
    void run();

    static std::atomic<bool> running_;
    static std::atomic<bool> dump_snapshot_;

  private:
    // Setup
    void init_world();
    void schedule_resource_respawn();
    void schedule_food_consumption(core::PlayerId id);

    // Network callbacks
    void on_client_connect(int fd);
    void on_client_line(int fd, std::string line);
    void on_client_disconnect(int fd);

    // Handshake
    void handle_handshake_line(int fd, std::string_view line);
    void complete_ai_handshake(int fd, core::TeamId team_id, std::string_view team_name);
    void complete_gui_handshake(int fd);
    void send_gui_snapshot(int fd);

    // AI command pipeline
    void enqueue_ai_command(int fd, std::string line);
    void execute_next_command(int fd);
    void dispatch_command(int fd, const protocol::ParsedCommand &cmd);

    // AI command handlers (called from scheduler callbacks)
    void cmd_forward(int fd);
    void cmd_right(int fd);
    void cmd_left(int fd);
    void cmd_look(int fd);
    void cmd_inventory(int fd);
    void cmd_broadcast(int fd, std::string text);
    void cmd_connect_nbr(int fd);
    void cmd_fork(int fd);
    void cmd_eject(int fd);
    void cmd_take(int fd, int resource_index);
    void cmd_set(int fd, int resource_index);
    void cmd_incantation(int fd);
    void cmd_incantation_complete(core::PlayerId initiator_id, std::vector<core::PlayerId> participants, int x, int y,
                                  int level);

    // GUI request dispatch
    void handle_gui_request(int fd, std::string_view line);

    // Broadcast a text message from sender to all other AI clients
    void broadcast_message_to_ai(core::PlayerId sender_id, std::string_view text);

    // Player lifecycle
    void kill_player(core::PlayerId id);

    // Timing helpers
    [[nodiscard]] core::EventScheduler::Tick now_ticks() const noexcept;
    [[nodiscard]] int ms_until_next_event() const noexcept;

    // State
    ServerArgs args_;
    core::WorldState world_;
    core::EventScheduler scheduler_;
    net::NetworkLayer net_;

    std::unordered_map<int, core::PlayerId> fd_to_player_;
    std::unordered_map<core::PlayerId, int> player_to_fd_;
    std::unordered_set<int> gui_fds_;

    struct IncantState
    {
        std::uint64_t event_id{0};
        std::vector<core::PlayerId> participants;
        int x{0}, y{0}, level{0};
    };
    std::unordered_map<core::PlayerId, IncantState> incantations_;

    std::chrono::steady_clock::time_point start_time_;
    std::mt19937 rng_;
};

} // namespace zappy::runtime
