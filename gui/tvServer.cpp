#include "tvServer.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <dlfcn.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace
{

constexpr const char *kDashboard = R"HTML(<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Zappy TV — Direct</title>
<style>
*{box-sizing:border-box}body{margin:0;min-height:100vh;background:radial-gradient(circle at 50% -20%,#43236d 0,#111326 48%,#070914 100%);color:#f7f4ff;font-family:system-ui,-apple-system,sans-serif}
main{width:min(900px,92vw);margin:auto;padding:28px 0 60px}.live{display:inline-flex;gap:8px;align-items:center;color:#ff637e;font-weight:900;letter-spacing:.16em}.dot{width:10px;height:10px;border-radius:50%;background:#ff3f61;box-shadow:0 0 18px #ff3f61;animation:p 1.2s infinite}@keyframes p{50%{opacity:.35}}
h1{margin:10px 0 4px;font-size:clamp(42px,11vw,86px);line-height:.9;background:linear-gradient(90deg,#fff,#d6a5ff,#72e7ff);color:transparent;background-clip:text}.sub{color:#9ca7c7;margin-bottom:28px}
.hero,.team{background:linear-gradient(145deg,rgba(255,255,255,.10),rgba(255,255,255,.035));border:1px solid rgba(255,255,255,.14);box-shadow:0 18px 60px #0007;border-radius:22px}
.hero{display:grid;grid-template-columns:repeat(3,1fr);padding:18px;margin-bottom:18px}.stat{text-align:center}.value{font-size:32px;font-weight:900}.label{color:#9ca7c7;font-size:12px;text-transform:uppercase;letter-spacing:.12em}
#teams{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:14px}.team{padding:19px;position:relative;overflow:hidden}.team:before{content:"";position:absolute;inset:0 auto 0 0;width:5px;background:var(--c)}.name{font-size:24px;font-weight:850}.meta{display:flex;justify-content:space-between;color:#bac3dd;margin-top:12px}.level{color:#ffd96a}
.team.leader{border-color:var(--c);box-shadow:0 0 30px color-mix(in srgb,var(--c) 35%,transparent)}.chance{display:flex;justify-content:space-between;align-items:end;margin-top:18px}.chance b{font-size:28px}.bar{height:9px;border-radius:9px;background:#080a13;margin-top:6px;overflow:hidden}.fill{height:100%;width:0;background:var(--c);box-shadow:0 0 14px var(--c);transition:width .8s ease}
.vote{width:100%;margin-top:16px;padding:11px;border:1px solid var(--c);border-radius:12px;background:transparent;color:#fff;font-weight:850;cursor:pointer}.vote.selected{background:var(--c);color:#080914}.vote:disabled{cursor:not-allowed;opacity:.45}.vote.selected:disabled{opacity:1}.bets{color:#8994b3;font-size:13px;margin-top:9px}
.section-title{margin:28px 0 12px;font-size:18px;letter-spacing:.12em;color:#cbd4f3}.mybet{display:none;margin:0 0 18px;padding:13px 16px;border-radius:13px;background:#18213d;color:#c8d7ff}.mybet.on{display:block}
.winner{display:none;margin:18px 0;padding:18px;border-radius:18px;text-align:center;background:linear-gradient(90deg,#6b3d11,#d29424,#6b3d11);font-size:25px;font-weight:900}.winner.on{display:block}
canvas{position:fixed;inset:0;width:100%;height:100%;pointer-events:none;z-index:10}footer{text-align:center;color:#747e9c;margin-top:28px;font-size:13px}@media(max-width:500px){.hero{padding:14px 5px}.value{font-size:25px}}
</style>
</head>
<body><main>
<div class="live"><span class="dot"></span> EN DIRECT</div>
<h1>ZAPPY TV</h1><div class="sub">Le tableau de bord officiel de la simulation</div>
<div id="winner" class="winner"></div>
<div id="mybet" class="mybet"></div>
<section class="hero">
 <div class="stat"><div class="value" id="players">—</div><div class="label">Joueurs</div></div>
 <div class="stat"><div class="value" id="eggs">—</div><div class="label">Œufs</div></div>
 <div class="stat"><div class="value" id="world">—</div><div class="label">Monde</div></div>
</section>
<h2 class="section-title">PRONOSTIC EN DIRECT</h2>
<section id="teams"></section>
<footer>Actualisation automatique • Zappy TV</footer>
</main><canvas id="confetti"></canvas>
<script>
const colors=['#ff536b','#4ca0ff','#42d484','#ffd34e','#b477ff','#ff963f','#43dadd','#ff72b5'];
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let voter=localStorage.zappyVoter;if(!voter){voter=crypto.randomUUID?crypto.randomUUID():Date.now()+'-'+Math.random();localStorage.zappyVoter=voter}
let pick='',celebrated=false;
async function vote(team){if(pick)return;const r=await fetch('/api/bet',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({voter,team})});if(r.ok)refresh()}
function celebrate(){if(celebrated)return;celebrated=true;const c=confetti,x=c.getContext('2d');c.width=innerWidth;c.height=innerHeight;
 const bits=Array.from({length:180},()=>({x:Math.random()*c.width,y:-Math.random()*c.height*.5,v:2+Math.random()*5,r:3+Math.random()*6,a:Math.random()*6.28,w:Math.random()*3-1.5,col:colors[Math.floor(Math.random()*colors.length)]}));
 let frames=0;(function draw(){x.clearRect(0,0,c.width,c.height);bits.forEach(b=>{b.y+=b.v;b.x+=Math.sin(b.a+=.08)*2+b.w;x.fillStyle=b.col;x.fillRect(b.x,b.y,b.r,b.r*1.8)});if(frames++<300)requestAnimationFrame(draw);else x.clearRect(0,0,c.width,c.height)})()}
async function refresh(){try{const r=await fetch('/api/state?voter='+encodeURIComponent(voter),{cache:'no-store'}),s=await r.json();pick=s.myVote||'';
players.textContent=s.totalPlayers;eggs.textContent=s.eggs;world.textContent=(s.season||'—')+' · '+(s.weather||'—');
const best=Math.max(...s.teams.map(t=>t.chance),0);teams.innerHTML=s.teams.map((t,i)=>`<article class="team ${t.chance===best&&best>0?'leader':''}" style="--c:${colors[i%colors.length]}"><div class="name">${esc(t.name)}</div><div class="meta"><span>${t.players} joueur${t.players>1?'s':''}</span><b class="level">NIVEAU ${t.maxLevel}</b></div><div class="chance"><span>Chance de victoire</span><b>${t.chance}%</b></div><div class="bar"><div class="fill" style="width:${t.chance}%"></div></div><button class="vote ${pick===t.name?'selected':''}" data-team="${encodeURIComponent(t.name)}" ${pick?'disabled':''}>${pick===t.name?'✓ MON ÉQUIPE':pick?'PARI VERROUILLÉ':'PARIER SUR '+esc(t.name).toUpperCase()}</button><div class="bets">${t.bets} vote${t.bets>1?'s':''} du public</div></article>`).join('');document.querySelectorAll('.vote:not(:disabled)').forEach(b=>b.onclick=()=>vote(decodeURIComponent(b.dataset.team)));
winner.textContent=s.winner?'🏆 '+s.winner.toUpperCase()+' REMPORTE LA PARTIE':'';winner.classList.toggle('on',!!s.winner);
mybet.textContent=pick?'Votre pronostic : '+pick:'';mybet.classList.toggle('on',!!pick);if(s.winner&&pick===s.winner)celebrate();
}catch(e){world.textContent='Hors ligne'}}refresh();setInterval(refresh,1000);
</script></body></html>)HTML";

std::string jsonEscape(const std::string &value)
{
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char c : value)
    {
        if (c == '"' || c == '\\')
            out += std::string("\\") + static_cast<char>(c);
        else if (c == '\n')
            out += "\\n";
        else if (c >= 0x20)
            out += static_cast<char>(c);
    }
    return out;
}

std::string jsonField(const std::string &body, const std::string &name)
{
    const std::string key = "\"" + name + "\"";
    std::size_t pos = body.find(key);
    if (pos == std::string::npos)
        return {};
    pos = body.find(':', pos + key.size());
    if (pos == std::string::npos)
        return {};
    pos = body.find('"', pos + 1);
    if (pos == std::string::npos)
        return {};
    const std::size_t end = body.find('"', pos + 1);
    if (end == std::string::npos)
        return {};
    return body.substr(pos + 1, end - pos - 1);
}

std::string localAddress()
{
    ifaddrs *interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0)
        return "127.0.0.1";
    std::string result = "127.0.0.1";
    for (const ifaddrs *it = interfaces; it != nullptr; it = it->ifa_next)
    {
        if (it->ifa_addr == nullptr || it->ifa_addr->sa_family != AF_INET || (it->ifa_flags & IFF_LOOPBACK) != 0 ||
            (it->ifa_flags & IFF_UP) == 0)
            continue;
        char address[INET_ADDRSTRLEN]{};
        const auto *in = reinterpret_cast<const sockaddr_in *>(it->ifa_addr);
        if (inet_ntop(AF_INET, &in->sin_addr, address, sizeof(address)) != nullptr)
        {
            result = address;
            break;
        }
    }
    freeifaddrs(interfaces);
    return result;
}

bool sendAll(int fd, const std::string &data)
{
    std::size_t sent = 0;
    while (sent < data.size())
    {
        const ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0)
            return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

} // namespace

TvServer::TvServer() = default;

TvServer::~TvServer()
{
    stop();
}

bool TvServer::start(std::uint16_t preferredPort)
{
    if (_running)
        return true;

    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0)
        return false;
    int reuse = 1;
    setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    bool bound = false;
    for (unsigned int offset = 0; offset < 20; ++offset)
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(static_cast<std::uint16_t>(preferredPort + offset));
        if (bind(_listenFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0)
        {
            _port = static_cast<std::uint16_t>(preferredPort + offset);
            bound = true;
            break;
        }
    }
    if (!bound || listen(_listenFd, 8) != 0)
    {
        close(_listenFd);
        _listenFd = -1;
        return false;
    }

    _url = "http://" + localAddress() + ":" + std::to_string(_port);
    buildQr();
    _running = true;
    _thread = std::thread(&TvServer::serveLoop, this);
    return true;
}

void TvServer::stop()
{
    if (!_running.exchange(false))
        return;
    if (_listenFd >= 0)
    {
        shutdown(_listenFd, SHUT_RDWR);
        close(_listenFd);
        _listenFd = -1;
    }
    if (_thread.joinable())
        _thread.join();
}

void TvServer::publish(TvSnapshot snapshot)
{
    std::lock_guard<std::mutex> lock(_snapshotMutex);
    _snapshot = std::move(snapshot);
}

bool TvServer::running() const
{
    return _running;
}

const std::string &TvServer::url() const
{
    return _url;
}

const std::vector<std::uint8_t> &TvServer::qrModules() const
{
    return _qr;
}

int TvServer::qrWidth() const
{
    return _qrWidth;
}

std::string TvServer::stateJson(const std::string &voter) const
{
    std::lock_guard<std::mutex> lock(_snapshotMutex);
    const TvSnapshot &snapshot = _snapshot;
    int totalScore = 0;
    for (const auto &team : snapshot.teams)
        totalScore += team.players * 10 + team.levelSum * 5 + team.maxLevel * 3;

    std::ostringstream json;
    const auto ownVote = _votesByVoter.find(voter);
    json << "{\"totalPlayers\":" << snapshot.totalPlayers << ",\"eggs\":" << snapshot.eggs
         << ",\"frequency\":" << snapshot.frequency << ",\"season\":\"" << jsonEscape(snapshot.season)
         << "\",\"weather\":\"" << jsonEscape(snapshot.weather) << "\",\"winner\":\""
         << jsonEscape(snapshot.winner) << "\",\"myVote\":\""
         << (ownVote == _votesByVoter.end() ? "" : jsonEscape(ownVote->second)) << "\",\"teams\":[";
    for (std::size_t i = 0; i < snapshot.teams.size(); ++i)
    {
        if (i != 0)
            json << ',';
        const auto &team = snapshot.teams[i];
        int bets = 0;
        for (const auto &[voter, votedTeam] : _votesByVoter)
        {
            (void)voter;
            if (votedTeam == team.name)
                ++bets;
        }
        const int score = team.players * 10 + team.levelSum * 5 + team.maxLevel * 3;
        const int chance = totalScore > 0
                               ? static_cast<int>((static_cast<long long>(score) * 100 + totalScore / 2) / totalScore)
                               : (snapshot.teams.empty() ? 0 : 100 / static_cast<int>(snapshot.teams.size()));
        json << "{\"name\":\"" << jsonEscape(team.name) << "\",\"players\":" << team.players
             << ",\"maxLevel\":" << team.maxLevel << ",\"chance\":" << chance << ",\"bets\":" << bets << '}';
    }
    json << "]}";
    return json.str();
}

bool TvServer::registerVote(const std::string &voter, const std::string &team)
{
    if (voter.empty() || voter.size() > 128 || team.empty() || team.size() > 128)
        return false;
    std::lock_guard<std::mutex> lock(_snapshotMutex);
    const auto found = std::find_if(_snapshot.teams.begin(), _snapshot.teams.end(),
                                    [&team](const TvTeamSnapshot &candidate) { return candidate.name == team; });
    if (found == _snapshot.teams.end())
        return false;
    const auto previousVote = _votesByVoter.find(voter);
    if (previousVote != _votesByVoter.end())
        return previousVote->second == team;
    _votesByVoter.emplace(voter, team);
    return true;
}

void TvServer::serveLoop()
{
    while (_running)
    {
        pollfd ready{_listenFd, POLLIN, 0};
        if (poll(&ready, 1, 250) <= 0)
            continue;
        const int client = accept(_listenFd, nullptr, nullptr);
        if (client < 0)
            continue;

        timeval timeout{1, 0};
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        char request[2048]{};
        const ssize_t count = recv(client, request, sizeof(request) - 1, 0);
        std::string method;
        std::string path = "/";
        std::string body;
        if (count > 0)
        {
            const std::string raw(request, static_cast<std::size_t>(count));
            std::istringstream firstLine(raw);
            firstLine >> method >> path;
            const std::size_t bodyStart = raw.find("\r\n\r\n");
            if (bodyStart != std::string::npos)
                body = raw.substr(bodyStart + 4);
        }

        std::string responseBody;
        std::string contentType;
        std::string status = "200 OK";
        if (method == "GET" && (path == "/" || path == "/index.html"))
        {
            responseBody = kDashboard;
            contentType = "text/html; charset=utf-8";
        }
        else if (method == "GET" && path.rfind("/api/state", 0) == 0)
        {
            std::string voter;
            const std::string marker = "voter=";
            const std::size_t voterPos = path.find(marker);
            if (voterPos != std::string::npos)
                voter = path.substr(voterPos + marker.size());
            responseBody = stateJson(voter);
            contentType = "application/json; charset=utf-8";
        }
        else if (method == "POST" && path == "/api/bet")
        {
            const bool accepted = registerVote(jsonField(body, "voter"), jsonField(body, "team"));
            status = accepted ? "200 OK" : "400 Bad Request";
            responseBody = accepted ? "{\"ok\":true}" : "{\"ok\":false}";
            contentType = "application/json; charset=utf-8";
        }
        else
        {
            status = "404 Not Found";
            responseBody = "Not found\n";
            contentType = "text/plain; charset=utf-8";
        }
        const std::string header = "HTTP/1.1 " + status + "\r\nContent-Type: " + contentType +
                                   "\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: " +
                                   std::to_string(responseBody.size()) + "\r\n\r\n";
        sendAll(client, header + responseBody);
        close(client);
    }
}

bool TvServer::buildQr()
{
    // libqrencode's tiny C ABI is loaded dynamically: machines without the
    // optional runtime still build and get a usable URL instead of a hard
    // dependency failure.
    struct QrCode
    {
        int version;
        int width;
        unsigned char *data;
    };
    using EncodeFn = QrCode *(*)(const char *, int, int);
    using FreeFn = void (*)(QrCode *);

    void *library = dlopen("libqrencode.so.4", RTLD_LAZY);
    if (library == nullptr)
        library = dlopen("libqrencode.so", RTLD_LAZY);
    if (library == nullptr)
        return false;
    auto encode = reinterpret_cast<EncodeFn>(dlsym(library, "QRcode_encodeString8bit"));
    auto release = reinterpret_cast<FreeFn>(dlsym(library, "QRcode_free"));
    if (encode == nullptr || release == nullptr)
    {
        dlclose(library);
        return false;
    }

    QrCode *code = encode(_url.c_str(), 0, 0); // version auto, error correction L
    if (code != nullptr && code->width > 0)
    {
        _qrWidth = code->width;
        _qr.resize(static_cast<std::size_t>(_qrWidth * _qrWidth));
        for (std::size_t i = 0; i < _qr.size(); ++i)
            _qr[i] = (code->data[i] & 1U) != 0U ? 1U : 0U;
        release(code);
    }
    dlclose(library);
    return !_qr.empty();
}
