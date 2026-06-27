#include "netClient.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

NetClient::~NetClient()
{
    if (_fd >= 0)
        ::close(_fd);
}

bool NetClient::connect(const std::string &host, int port)
{
    if (port <= 0 || port > 65535)
        return false;

    _fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_fd < 0)
        return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    // Accept a dotted-quad directly; otherwise resolve a hostname.
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
    {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo *res = nullptr;
        if (::getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || res == nullptr)
        {
            ::close(_fd);
            _fd = -1;
            return false;
        }
        addr.sin_addr = reinterpret_cast<sockaddr_in *>(res->ai_addr)->sin_addr;
        ::freeaddrinfo(res);
    }

    if (::connect(_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        ::close(_fd);
        _fd = -1;
        return false;
    }
    return true;
}

std::string NetClient::readLineBlocking()
{
    // Serve a buffered line first if one is already complete.
    for (;;)
    {
        auto nl = _buf.find('\n');
        if (nl != std::string::npos)
        {
            std::string line = _buf.substr(0, nl);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            _buf.erase(0, nl + 1);
            return line;
        }
        char tmp[4096];
        ssize_t n = ::read(_fd, tmp, sizeof(tmp));
        if (n <= 0)
        {
            _closed = true;
            return {};
        }
        _buf.append(tmp, static_cast<size_t>(n));
    }
}

bool NetClient::handshake(int &outWidth, int &outHeight)
{
    // Server greets with WELCOME.
    std::string greet = readLineBlocking();
    if (greet != "WELCOME")
        return false;

    // Identify as a graphical client.
    send("GRAPHIC");

    // The snapshot starts streaming; the size header msz is what we need before
    // building the map. Ignore anything ahead of it (there shouldn't be any).
    for (;;)
    {
        std::string line = readLineBlocking();
        if (_closed)
            return false;
        if (line.rfind("msz ", 0) == 0)
        {
            int w = 0, h = 0;
            if (std::sscanf(line.c_str(), "msz %d %d", &w, &h) == 2 && w > 0 && h > 0)
            {
                outWidth = w;
                outHeight = h;
                // Switch to non-blocking now; the rest of the snapshot (tna/bct/
                // pnw/enw) plus any already-buffered bytes get drained by poll().
                int flags = ::fcntl(_fd, F_GETFL, 0);
                if (flags >= 0)
                    ::fcntl(_fd, F_SETFL, flags | O_NONBLOCK);
                return true;
            }
            return false;
        }
    }
}

std::vector<std::string> NetClient::drainLines()
{
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i < _buf.size(); ++i)
    {
        if (_buf[i] == '\n')
        {
            std::string line = _buf.substr(start, i - start);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(std::move(line));
            start = i + 1;
        }
    }
    _buf.erase(0, start);
    return lines;
}

std::vector<std::string> NetClient::poll()
{
    if (_fd < 0 || _closed)
        return drainLines(); // hand back anything already buffered, then nothing

    for (;;)
    {
        char tmp[4096];
        ssize_t n = ::recv(_fd, tmp, sizeof(tmp), 0);
        if (n > 0)
        {
            _buf.append(tmp, static_cast<size_t>(n));
            if (static_cast<size_t>(n) < sizeof(tmp))
                break; // drained the socket for now
            continue;  // full read, likely more waiting
        }
        if (n == 0)
        {
            _closed = true; // peer closed
            break;
        }
        // n < 0
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break; // nothing more right now
        if (errno == EINTR)
            continue;
        _closed = true; // hard error
        break;
    }
    return drainLines();
}

void NetClient::send(std::string_view line)
{
    if (_fd < 0)
        return;
    std::string msg(line);
    msg += '\n';
    ssize_t off = 0;
    while (off < static_cast<ssize_t>(msg.size()))
    {
        ssize_t n = ::write(_fd, msg.data() + off, msg.size() - static_cast<size_t>(off));
        if (n <= 0)
        {
            if (n < 0 && (errno == EINTR))
                continue;
            _closed = true;
            return;
        }
        off += n;
    }
}
