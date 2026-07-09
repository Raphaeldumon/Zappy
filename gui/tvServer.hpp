#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct TvTeamSnapshot
{
    std::string name;
    int players{0};
    int maxLevel{0};
    int levelSum{0};
};

struct TvSnapshot
{
    std::vector<TvTeamSnapshot> teams;
    int totalPlayers{0};
    int eggs{0};
    int frequency{0};
    std::string season;
    std::string weather;
    std::string winner;
};

// Tiny read-only HTTP server for the presentation dashboard. It deliberately
// has no dependency on the game server or raylib: the GUI publishes immutable
// snapshots and phones on the same Wi-Fi fetch /api/state once per second.
class TvServer
{
  public:
    TvServer();
    ~TvServer();

    TvServer(const TvServer &) = delete;
    TvServer &operator=(const TvServer &) = delete;

    bool start(std::uint16_t preferredPort = 8080);
    void stop();
    void publish(TvSnapshot snapshot);

    bool running() const;
    const std::string &url() const;

    // QR modules, row-major. Values are 0 (white) or 1 (black). Empty means
    // libqrencode is unavailable; the overlay still exposes the typed URL.
    const std::vector<std::uint8_t> &qrModules() const;
    int qrWidth() const;

  private:
    void serveLoop();
    std::string stateJson(const std::string &voter) const;
    bool registerVote(const std::string &voter, const std::string &team);
    bool buildQr();

    std::atomic<bool> _running{false};
    int _listenFd{-1};
    std::uint16_t _port{0};
    std::string _url;
    std::thread _thread;
    mutable std::mutex _snapshotMutex;
    TvSnapshot _snapshot;
    std::unordered_map<std::string, std::string> _votesByVoter;
    std::vector<std::uint8_t> _qr;
    int _qrWidth{0};
};
