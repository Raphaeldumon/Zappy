#pragma once

#include <string>
#include <string_view>
#include <vector>

// Minimal blocking-connect / non-blocking-stream TCP client for the Zappy GUI
// protocol. Owns the socket fd. Connect + handshake run blocking once at
// startup; thereafter poll() is called every frame and never blocks.
//
// Line discipline mirrors the server's net layer (server/net/client.cpp):
// recv into a std::string buffer, then split off complete '\n'-terminated lines.
class NetClient {
public:
    NetClient() = default;
    ~NetClient();

    NetClient(const NetClient&)            = delete;
    NetClient& operator=(const NetClient&) = delete;

    // Open a TCP connection. Returns false on any failure (caller exits 84).
    bool connect(const std::string& host, int port);

    // GRAPHIC handshake: read until WELCOME, send "GRAPHIC", then read until the
    // first "msz W H" and return W/H. Leftover snapshot bytes stay buffered for
    // the first poll(). Returns false on protocol/IO error. Blocking.
    bool handshake(int& outWidth, int& outHeight);

    // Drain whatever is readable now (non-blocking) and return complete lines.
    // Sets closed() on EOF or a hard socket error.
    std::vector<std::string> poll();

    // Queue-free direct send; appends '\n'. Used for GUI requests (e.g. sst).
    void send(std::string_view line);

    [[nodiscard]] bool closed() const { return _closed; }

private:
    // Pull complete '\n' lines out of _buf, stripping a trailing '\r'.
    std::vector<std::string> drainLines();
    // Blocking read of a single line (handshake only). Empty string on error.
    std::string readLineBlocking();

    int         _fd{-1};
    std::string _buf;
    bool        _closed{false};
};
