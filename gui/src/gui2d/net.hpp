#pragma once

#include <string>
#include <vector>

namespace zappy::gui2d
{

// Blocking TCP connect, then non-blocking line reads. POSIX sockets only — this is
// the throwaway debug GUI, not the production client.
class NetClient
{
  public:
    ~NetClient();

    // Connect and send the "GRAPHIC\n" handshake. Returns false on failure
    // (err filled with a human-readable reason).
    bool connect(const std::string &host, int port, std::string &err);

    // Pull all complete '\n'-terminated lines currently buffered on the socket.
    // Appends them (without the '\n') to `out`. Returns false if the peer closed.
    bool poll_lines(std::vector<std::string> &out);

    // Send a raw request line (we append '\n').
    void send_line(const std::string &line);

    bool connected() const { return fd_ >= 0; }

  private:
    int fd_{-1};
    std::string rx_; // accumulates partial line data
};

} // namespace zappy::gui2d
