// zappy_gui2d — throwaway 2D debug viewer.
//
// Purpose: SEE the server. Connects as a GRAPHIC client, parses the GUI protocol
// and draws the map with raylib. Not the final renderer (that's the Vulkan one).
//
// Usage: ./zappy_gui2d -p <port> -h <host>
//
//   * grid of tiles, brightness = total resources on the tile
//   * coloured square per player with a heading triangle (N/E/S/W)
//   * yellow dots for eggs
//   * HUD: connection state, map size, time unit, player/egg counts
//
// If the server isn't reachable it still opens the window and shows "connecting…"
// so you can launch it in any order.

#include "net.hpp"
#include "parser.hpp"
#include "world.hpp"

#include "raylib.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr int EXIT_ERR = 84;
constexpr int WIN_W = 1100;
constexpr int WIN_H = 800;
constexpr int HUD_W = 260; // right-side panel

void print_usage(const char *prog)
{
    std::printf("USAGE: %s -p port -h machine\n"
                "  -p port      server port (default 4242)\n"
                "  -h machine   server host (default localhost)\n"
                "  --help       show this help\n",
                prog);
}

// Stable colour per player id so the same drone keeps its colour.
Color player_color(std::uint32_t id)
{
    static const Color palette[] = {RED, BLUE, GREEN, PURPLE, ORANGE, PINK, SKYBLUE, LIME, GOLD, VIOLET};
    return palette[id % (sizeof(palette) / sizeof(palette[0]))];
}

// Distinct colour per resource index (0..6), matching gui2d::resource_name().
// Shown as dots on each tile and in the HUD legend.
Color resource_color(int i)
{
    static const Color c[zappy::gui2d::RESOURCE_COUNT] = {
        Color{235, 235, 235, 255}, // food      — white
        Color{150, 150, 160, 255}, // linemate  — grey
        Color{90, 140, 255, 255},  // deraumere — blue
        Color{60, 200, 120, 255},  // sibur     — green
        Color{220, 90, 200, 255},  // mendiane  — magenta
        Color{240, 160, 40, 255},  // phiras    — orange
        Color{230, 70, 70, 255},   // thystame  — red
    };
    return (i >= 0 && i < zappy::gui2d::RESOURCE_COUNT) ? c[i] : WHITE;
}

// Draw the 7 resource types as dots in a fixed 4x2 slot grid inside a tile.
// A slot is drawn only when that resource is present; a small count is shown
// next to it when there's more than one. Fixed slots keep colours readable
// across tiles (food is always top-left, etc.).
void draw_tile_resources(float fx, float fy, float cell, const int resources[zappy::gui2d::RESOURCE_COUNT])
{
    if (cell < 14.0f)
    {
        // Too small for dots — fall back to a single total label.
        int total = 0;
        for (int i = 0; i < zappy::gui2d::RESOURCE_COUNT; ++i)
            total += resources[i];
        if (total > 0)
            DrawText(TextFormat("%d", total), static_cast<int>(fx) + 3, static_cast<int>(fy) + 3, 10,
                     Color{200, 200, 200, 255});
        return;
    }

    constexpr int COLS = 4;
    constexpr int ROWS = 2;
    const float pad = cell * 0.12f;
    const float gw = cell - 2.0f * pad;
    const float gh = cell - 2.0f * pad;
    const float dot = (gw / COLS) * 0.32f;

    for (int i = 0; i < zappy::gui2d::RESOURCE_COUNT; ++i)
    {
        if (resources[i] <= 0)
            continue;
        int col = i % COLS;
        int row = i / COLS;
        float dx = fx + pad + (static_cast<float>(col) + 0.5f) * (gw / COLS);
        float dy = fy + pad + (static_cast<float>(row) + 0.5f) * (gh / ROWS);
        DrawCircle(static_cast<int>(dx), static_cast<int>(dy), dot, resource_color(i));
        if (resources[i] > 1 && cell > 30.0f)
            DrawText(TextFormat("%d", resources[i]), static_cast<int>(dx + dot + 1.0f), static_cast<int>(dy - dot), 9,
                     Color{210, 210, 210, 255});
    }
}

// Draw a heading triangle inside a player cell. o: 1=N 2=E 3=S 4=W.
void draw_heading(float cx, float cy, float r, int o, Color c)
{
    Vector2 tip{cx, cy}, a{cx, cy}, b{cx, cy};
    switch (o)
    {
    case 1: // North
        tip = {cx, cy - r};
        a = {cx - r * 0.7f, cy + r * 0.4f};
        b = {cx + r * 0.7f, cy + r * 0.4f};
        break;
    case 2: // East
        tip = {cx + r, cy};
        a = {cx - r * 0.4f, cy - r * 0.7f};
        b = {cx - r * 0.4f, cy + r * 0.7f};
        break;
    case 3: // South
        tip = {cx, cy + r};
        a = {cx + r * 0.7f, cy - r * 0.4f};
        b = {cx - r * 0.7f, cy - r * 0.4f};
        break;
    default: // West
        tip = {cx - r, cy};
        a = {cx + r * 0.4f, cy + r * 0.7f};
        b = {cx + r * 0.4f, cy - r * 0.7f};
        break;
    }
    DrawTriangle(tip, a, b, c);
}

} // namespace

int main(int argc, char **argv)
{
    int port = 4242;
    std::string host = "localhost";

    for (int i = 1; i < argc; ++i)
    {
        std::string flag = argv[i];
        if (flag == "--help")
        {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (i + 1 >= argc)
        {
            std::fprintf(stderr, "zappy_gui2d: option %s expects a value\n", flag.c_str());
            return EXIT_ERR;
        }
        const char *val = argv[++i];
        if (flag == "-p")
            port = std::atoi(val);
        else if (flag == "-h")
            host = val;
        else
        {
            std::fprintf(stderr, "zappy_gui2d: unknown option %s\n", flag.c_str());
            return EXIT_ERR;
        }
    }

    zappy::gui2d::World world;
    zappy::gui2d::NetClient net;
    std::string err;
    bool connected = net.connect(host, port, err);
    std::string status = connected ? "connected" : ("connect failed: " + err);

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(WIN_W, WIN_H, "zappy_gui2d — debug viewer");
    SetTargetFPS(60);

    int unknown_lines = 0;

    while (!WindowShouldClose())
    {
        // --- network pump ---
        if (net.connected())
        {
            std::vector<std::string> lines;
            if (!net.poll_lines(lines))
            {
                status = "disconnected";
            }
            for (const auto &line : lines)
                if (!zappy::gui2d::apply_line(world, line))
                    ++unknown_lines;
        }

        // --- layout ---
        const int board_w = WIN_W - HUD_W;
        const int board_h = WIN_H;
        float cell = 0.0f;
        float ox = 0.0f, oy = 0.0f;
        if (world.width > 0 && world.height > 0)
        {
            float cw = static_cast<float>(board_w - 40) / static_cast<float>(world.width);
            float ch = static_cast<float>(board_h - 40) / static_cast<float>(world.height);
            cell = cw < ch ? cw : ch;
            ox = 20.0f + (static_cast<float>(board_w - 40) - cell * static_cast<float>(world.width)) / 2.0f;
            oy = 20.0f + (static_cast<float>(board_h - 40) - cell * static_cast<float>(world.height)) / 2.0f;
        }

        // --- draw ---
        BeginDrawing();
        ClearBackground(Color{18, 18, 24, 255});

        // tiles
        for (int y = 0; y < world.height; ++y)
        {
            for (int x = 0; x < world.width; ++x)
            {
                zappy::gui2d::Tile *tile = world.tile_at(x, y);
                float fx = ox + static_cast<float>(x) * cell;
                float fy = oy + static_cast<float>(y) * cell;
                // Flat dark cell; resources are shown as coloured dots on top.
                DrawRectangle(static_cast<int>(fx), static_cast<int>(fy), static_cast<int>(cell) - 1,
                              static_cast<int>(cell) - 1, Color{28, 30, 38, 255});
                if (tile)
                    draw_tile_resources(fx, fy, cell, tile->resources);
            }
        }

        // eggs
        for (const auto &[id, e] : world.eggs)
        {
            (void)id;
            float cx = ox + (static_cast<float>(e.x) + 0.5f) * cell;
            float cy = oy + (static_cast<float>(e.y) + 0.5f) * cell;
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), cell * 0.12f, YELLOW);
        }

        // players
        for (const auto &[id, p] : world.players)
        {
            float cx = ox + (static_cast<float>(p.x) + 0.5f) * cell;
            float cy = oy + (static_cast<float>(p.y) + 0.5f) * cell;
            float r = cell * 0.35f;
            Color c = player_color(id);
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), r, Fade(c, 0.35f));
            draw_heading(cx, cy, r, p.orientation, c);
            DrawText(TextFormat("%u", id), static_cast<int>(cx) - 4, static_cast<int>(cy) - 4, 10, WHITE);
        }

        // --- HUD ---
        const int hx = board_w + 14;
        DrawRectangle(board_w, 0, HUD_W, WIN_H, Color{12, 12, 16, 255});
        DrawLine(board_w, 0, board_w, WIN_H, Color{60, 60, 70, 255});
        int hy = 16;
        auto line_out = [&](const char *txt, Color col) {
            DrawText(txt, hx, hy, 18, col);
            hy += 26;
        };
        line_out("zappy_gui2d", RAYWHITE);
        hy += 6;
        line_out(TextFormat("server: %s:%d", host.c_str(), port), LIGHTGRAY);
        line_out(TextFormat("status: %s", status.c_str()),
                 net.connected() ? Color{120, 220, 120, 255} : Color{220, 120, 120, 255});
        line_out(TextFormat("map: %d x %d", world.width, world.height), LIGHTGRAY);
        line_out(TextFormat("time unit: %d", world.time_unit), LIGHTGRAY);
        line_out(TextFormat("players: %zu", world.players.size()), LIGHTGRAY);
        line_out(TextFormat("eggs: %zu", world.eggs.size()), LIGHTGRAY);
        line_out(TextFormat("teams: %zu", world.teams.size()), LIGHTGRAY);
        line_out(TextFormat("unknown lines: %d", unknown_lines), unknown_lines ? Color{220, 180, 120, 255} : LIGHTGRAY);
        hy += 8;
        for (const auto &team : world.teams)
            line_out(TextFormat("- %s", team.c_str()), Color{150, 170, 220, 255});

        // resource legend (dot colour -> name -> total on the whole map)
        hy += 12;
        line_out("resources:", LIGHTGRAY);
        int res_totals[zappy::gui2d::RESOURCE_COUNT] = {};
        for (const auto &tile : world.tiles)
            for (int i = 0; i < zappy::gui2d::RESOURCE_COUNT; ++i)
                res_totals[i] += tile.resources[i];
        for (int i = 0; i < zappy::gui2d::RESOURCE_COUNT; ++i)
        {
            DrawCircle(hx + 6, hy + 8, 5, resource_color(i));
            DrawText(TextFormat("%s  (%d)", zappy::gui2d::resource_name(i), res_totals[i]), hx + 18, hy, 16, LIGHTGRAY);
            hy += 22;
        }

        if (!world.last_message.empty())
        {
            hy += 8;
            line_out("msg:", LIGHTGRAY);
            line_out(world.last_message.c_str(), Color{220, 220, 140, 255});
        }

        DrawFPS(board_w + 14, WIN_H - 24);
        EndDrawing();
    }

    CloseWindow();
    return EXIT_SUCCESS;
}
