#include "net/client.hpp"

#include <unistd.h>

#include <algorithm>

namespace zappy::net
{

std::vector<std::string> Client::drain_lines()
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i < recv_buf.size(); ++i)
    {
        if (recv_buf[i] == '\n')
        {
            std::string line = recv_buf.substr(start, i - start);
            // Strip trailing \r for Windows-style line endings
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(std::move(line));
            start = i + 1;
        }
    }
    recv_buf.erase(0, start);
    return lines;
}

void Client::enqueue(std::string_view line)
{
    std::string msg(line);
    msg += '\n';
    send_queue.push_back(std::move(msg));
}

void Client::enqueue_front(std::string_view line)
{
    std::string msg(line);
    msg += '\n';
    send_queue.push_front(std::move(msg));
}

int Client::fill_recv()
{
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0)
        recv_buf.append(buf, static_cast<std::size_t>(n));
    return static_cast<int>(n);
}

int Client::flush_writes()
{
    if (send_queue.empty())
        return 0;
    const std::string &front = send_queue.front();
    ssize_t n = write(fd, front.data(), front.size());
    if (n <= 0)
        return static_cast<int>(n);
    if (static_cast<std::size_t>(n) >= front.size())
    {
        send_queue.pop_front();
    }
    else
    {
        // Partial write: keep the remainder
        send_queue.front() = front.substr(static_cast<std::size_t>(n));
    }
    return static_cast<int>(n);
}

} // namespace zappy::net
