#include "net/network_layer.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace zappy::net
{

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl F_GETFL failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl F_SETFL O_NONBLOCK failed");
}

NetworkLayer::NetworkLayer(int port)
{
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0)
        throw std::runtime_error("socket() failed: " + std::string(strerror(errno)));

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed: " + std::string(strerror(errno)));

    if (listen(listen_fd_, SOMAXCONN) < 0)
        throw std::runtime_error("listen() failed: " + std::string(strerror(errno)));

    set_nonblocking(listen_fd_);
}

NetworkLayer::~NetworkLayer()
{
    for (auto &[fd, _] : clients_)
        close(fd);
    if (listen_fd_ >= 0)
        close(listen_fd_);
}

void NetworkLayer::accept_new_connection()
{
    int cfd = accept(listen_fd_, nullptr, nullptr);
    if (cfd < 0)
        return; // EAGAIN or error
    set_nonblocking(cfd);
    Client client{};
    client.fd = cfd;
    clients_.emplace(cfd, std::move(client));
    pollfds_dirty_ = true;
    if (on_connect_)
        on_connect_(cfd);
}

void NetworkLayer::rebuild_pollfds()
{
    pollfds_.clear();
    // Index 0: listen fd
    pollfds_.push_back({listen_fd_, POLLIN, 0});
    for (auto &[fd, client] : clients_)
    {
        short events = POLLIN;
        if (client.has_pending_write())
            events |= POLLOUT;
        pollfds_.push_back({fd, events, 0});
    }
    pollfds_dirty_ = false;
}

void NetworkLayer::poll_once(int timeout_ms)
{
    if (pollfds_dirty_)
        rebuild_pollfds();

    int n = poll(pollfds_.data(), static_cast<nfds_t>(pollfds_.size()), timeout_ms);
    if (n < 0)
    {
        if (errno == EINTR)
            return;
        return; // other errors: just continue
    }
    if (n == 0)
        return;

    if (pollfds_[0].revents & POLLIN)
        accept_new_connection();

    // Handle existing clients (iterate over a snapshot of fds to avoid iterator issues)
    std::vector<int> fds_snapshot;
    fds_snapshot.reserve(pollfds_.size());
    for (std::size_t i = 1; i < pollfds_.size(); ++i)
        fds_snapshot.push_back(pollfds_[i].fd);

    for (std::size_t i = 1; i < pollfds_.size() && i - 1 < fds_snapshot.size(); ++i)
    {
        int fd = fds_snapshot[i - 1];
        short revents = pollfds_[i].revents;
        auto it = clients_.find(fd);
        if (it == clients_.end())
            continue;
        auto &client = it->second;
        if (client.state == ClientState::Dead)
            continue;

        // Handle POLLIN before POLLHUP: remote may send data then close in one shot.
        // fill_recv returning 0 (EOF) marks client Dead; we still process any
        // buffered lines before sweeping.
        if (revents & POLLIN)
        {
            int ret = client.fill_recv();
            bool eof = (ret == 0);
            bool err = (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK);
            for (auto &line : client.drain_lines())
            {
                if (client.state != ClientState::Dead && on_line_)
                    on_line_(fd, std::move(line));
            }
            if (eof || err)
            {
                close_client(fd);
                continue;
            }
        }
        if (revents & (POLLERR | POLLNVAL))
        {
            close_client(fd);
            continue;
        }
        // POLLHUP: remote closed its write side (half-close). Flush remaining
        // send_queue first, then close.
        if (revents & POLLOUT)
        {
            client.flush_writes();
        }
        if ((revents & POLLHUP) && !client.has_pending_write())
        {
            close_client(fd);
        }
    }

    sweep_dead_clients();
}

void NetworkLayer::sweep_dead_clients()
{
    std::vector<int> to_remove;
    for (auto &[fd, client] : clients_)
        if (client.state == ClientState::Dead)
            to_remove.push_back(fd);
    for (int fd : to_remove)
    {
        if (on_disconnect_)
            on_disconnect_(fd);
        close(fd);
        clients_.erase(fd);
        pollfds_dirty_ = true;
    }
}

void NetworkLayer::send_to(int fd, std::string_view line)
{
    auto it = clients_.find(fd);
    if (it == clients_.end() || it->second.state == ClientState::Dead)
        return;
    it->second.enqueue(line);
    pollfds_dirty_ = true; // may need POLLOUT now
}

void NetworkLayer::broadcast_gui(std::string_view line)
{
    for (auto &[fd, client] : clients_)
    {
        if (client.state == ClientState::GUI)
        {
            client.enqueue(line);
            pollfds_dirty_ = true;
        }
    }
}

void NetworkLayer::broadcast_all(std::string_view line)
{
    for (auto &[fd, client] : clients_)
    {
        if (client.state == ClientState::AI || client.state == ClientState::GUI)
        {
            client.enqueue(line);
            pollfds_dirty_ = true;
        }
    }
}

void NetworkLayer::close_client(int fd)
{
    auto it = clients_.find(fd);
    if (it == clients_.end())
        return;
    it->second.state = ClientState::Dead;
    pollfds_dirty_ = true;
}

Client *NetworkLayer::find_client(int fd) noexcept
{
    auto it = clients_.find(fd);
    return it == clients_.end() ? nullptr : &it->second;
}

} // namespace zappy::net
