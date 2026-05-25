#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace zappy::net
{

enum class ClientState
{
    Handshake, // just connected, WELCOME sent, waiting for team name
    AI,        // authenticated as AI player
    GUI,       // authenticated as GUI client
    Dead,      // to be removed on next sweep
};

struct Client
{
    int fd{-1};
    ClientState state{ClientState::Handshake};
    uint32_t player_id{0}; // valid when state == AI

    std::string recv_buf;
    std::deque<std::string> send_queue;

    // AI only: command lines waiting for execution (max MAX_COMMAND_QUEUE)
    std::deque<std::string> command_queue;
    bool has_pending_action{false};

    // Extract complete \n-terminated lines from recv_buf.
    // Strips trailing \r if present. Removes consumed bytes.
    std::vector<std::string> drain_lines();

    // Queue a line to send (appends \n automatically).
    void enqueue(std::string_view line);

    // Prepend an urgent line (dead, eject: K, message K, text).
    void enqueue_front(std::string_view line);

    // Read from fd into recv_buf. Returns bytes read, 0 = EOF, -1 = error.
    int fill_recv();

    // Write from send_queue to fd (one syscall, partial-write safe).
    // Returns bytes written, -1 = error.
    int flush_writes();

    [[nodiscard]] bool has_pending_write() const noexcept
    {
        return !send_queue.empty();
    }
};

} // namespace zappy::net
