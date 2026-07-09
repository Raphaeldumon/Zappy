#include "../tvServer.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace
{

void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "test_tv_server: " << message << '\n';
        std::exit(1);
    }
}

std::string httpRequest(std::uint16_t port, const std::string &method, const std::string &path,
                        const std::string &body = {})
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    require(fd >= 0, "socket failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    require(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1, "inet_pton failed");
    require(connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0, "connect failed");
    const std::string request =
        method + " " + path +
        " HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    require(send(fd, request.data(), request.size(), 0) == static_cast<ssize_t>(request.size()), "send failed");

    std::string response;
    char buffer[4096];
    for (;;)
    {
        const ssize_t count = recv(fd, buffer, sizeof(buffer), 0);
        if (count <= 0)
            break;
        response.append(buffer, static_cast<std::size_t>(count));
    }
    close(fd);
    return response;
}

} // namespace

int main()
{
    TvServer server;
    require(server.start(18080), "server start failed");
    require(server.running(), "server is not running");
    require(server.url().find(":18080") != std::string::npos, "unexpected URL");
    require(server.qrWidth() > 0 && !server.qrModules().empty(), "QR generation failed");

    TvSnapshot snapshot;
    snapshot.totalPlayers = 3;
    snapshot.eggs = 2;
    snapshot.season = "winter";
    snapshot.weather = "storm";
    snapshot.teams.push_back({"rouge", 2, 4, 7});
    snapshot.teams.push_back({"bleu", 1, 2, 2});
    server.publish(snapshot);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const std::string page = httpRequest(18080, "GET", "/");
    require(page.find("200 OK") != std::string::npos, "dashboard status is not 200");
    require(page.find("ZAPPY TV") != std::string::npos, "dashboard body is missing");

    const std::string vote =
        httpRequest(18080, "POST", "/api/bet", "{\"voter\":\"phone-1\",\"team\":\"rouge\"}");
    require(vote.find("200 OK") != std::string::npos, "bet was rejected");

    const std::string api = httpRequest(18080, "GET", "/api/state?voter=phone-1");
    require(api.find("\"totalPlayers\":3") != std::string::npos, "player count missing from API");
    require(api.find("\"name\":\"rouge\"") != std::string::npos, "team missing from API");
    require(api.find("\"weather\":\"storm\"") != std::string::npos, "weather missing from API");
    require(api.find("\"chance\":") != std::string::npos, "live chance missing from API");
    require(api.find("\"bets\":1") != std::string::npos, "bet count missing from API");
    require(api.find("\"myVote\":\"rouge\"") != std::string::npos, "personal vote missing from API");

    const std::string otherPhone = httpRequest(18080, "GET", "/api/state?voter=phone-2");
    require(otherPhone.find("\"myVote\":\"\"") != std::string::npos, "vote leaked to another phone");

    const std::string movedVote =
        httpRequest(18080, "POST", "/api/bet", "{\"voter\":\"phone-1\",\"team\":\"bleu\"}");
    require(movedVote.find("400 Bad Request") != std::string::npos, "updated bet was accepted");
    const std::string movedApi = httpRequest(18080, "GET", "/api/state?voter=phone-1");
    const std::size_t red = movedApi.find("\"name\":\"rouge\"");
    require(red != std::string::npos && movedApi.find("\"bets\":1", red) != std::string::npos,
            "original bet was not preserved");

    const std::string repeatedVote =
        httpRequest(18080, "POST", "/api/bet", "{\"voter\":\"phone-1\",\"team\":\"rouge\"}");
    require(repeatedVote.find("200 OK") != std::string::npos, "repeated identical bet was rejected");

    server.stop();
    require(!server.running(), "server did not stop");
}
