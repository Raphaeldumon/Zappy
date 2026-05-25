#pragma once

#include "net/client.hpp"

#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

struct pollfd; // forward — caller includes <poll.h>

namespace zappy::net
{

using LineHandler = std::function<void(int fd, std::string line)>;
using ConnectHandler = std::function<void(int fd)>;
using DisconnectHandler = std::function<void(int fd)>;

class NetworkLayer
{
  public:
    explicit NetworkLayer(int port);
    ~NetworkLayer();

    NetworkLayer(const NetworkLayer &) = delete;
    NetworkLayer &operator=(const NetworkLayer &) = delete;

    void on_connect(ConnectHandler h)
    {
        on_connect_ = std::move(h);
    }
    void on_line(LineHandler h)
    {
        on_line_ = std::move(h);
    }
    void on_disconnect(DisconnectHandler h)
    {
        on_disconnect_ = std::move(h);
    }

    // Send a line to one client (appends \n).
    void send_to(int fd, std::string_view line);

    // Send a line to all GUI clients.
    void broadcast_gui(std::string_view line);

    // Send a line to all connected clients (AI + GUI).
    void broadcast_all(std::string_view line);

    // Mark client Dead; swept at end of current poll_once().
    void close_client(int fd);

    // Run one poll(2) iteration with the given timeout (ms, -1 = infinite).
    void poll_once(int timeout_ms);

    [[nodiscard]] Client *find_client(int fd) noexcept;
    [[nodiscard]] const std::unordered_map<int, Client> &clients() const noexcept
    {
        return clients_;
    }

  private:
    void accept_new_connection();
    void rebuild_pollfds();
    void sweep_dead_clients();

    int listen_fd_{-1};
    std::unordered_map<int, Client> clients_; // keyed by fd
    std::vector<struct pollfd> pollfds_;
    bool pollfds_dirty_{true};

    ConnectHandler on_connect_;
    LineHandler on_line_;
    DisconnectHandler on_disconnect_;
};

} // namespace zappy::net
