#include "net.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace zappy::gui2d
{

NetClient::~NetClient()
{
    if (fd_ >= 0)
        ::close(fd_);
}

bool NetClient::connect(const std::string &host, int port, std::string &err)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *res = nullptr;
    const std::string port_str = std::to_string(port);
    int rc = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (rc != 0)
    {
        err = std::string("getaddrinfo: ") + gai_strerror(rc);
        return false;
    }

    int fd = -1;
    for (addrinfo *ai = res; ai != nullptr; ai = ai->ai_next)
    {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(res);

    if (fd < 0)
    {
        err = std::string("connect: ") + std::strerror(errno);
        return false;
    }

    fd_ = fd;
    send_line("GRAPHIC");

    // Switch to non-blocking for the render-loop polling.
    int flags = ::fcntl(fd_, F_GETFL, 0);
    ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    return true;
}

void NetClient::send_line(const std::string &line)
{
    if (fd_ < 0)
        return;
    std::string buf = line;
    buf += '\n';
    ::send(fd_, buf.data(), buf.size(), MSG_NOSIGNAL);
}

bool NetClient::poll_lines(std::vector<std::string> &out)
{
    if (fd_ < 0)
        return false;

    char chunk[4096];
    for (;;)
    {
        ssize_t n = ::recv(fd_, chunk, sizeof(chunk), 0);
        if (n > 0)
        {
            rx_.append(chunk, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0)
        {
            ::close(fd_);
            fd_ = -1;
            return false; // peer closed
        }
        // n < 0
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break; // no more data right now
        if (errno == EINTR)
            continue;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    std::size_t pos;
    while ((pos = rx_.find('\n')) != std::string::npos)
    {
        std::string line = rx_.substr(0, pos);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        out.push_back(std::move(line));
        rx_.erase(0, pos + 1);
    }
    return true;
}

} // namespace zappy::gui2d
