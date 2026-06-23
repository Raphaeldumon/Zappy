#pragma once

#include <array>
#include <cstdint> // For uint32_t
#include <string_view>
#include <vector>

// Using resource count from zappy::protocol::ai for consistency with the project's protocol.
// This ensures the resource indices and counts align across different components.
inline constexpr int MAP_RESOURCE_COUNT = 7;

// Names for resources, useful for debugging or UI display.
inline constexpr std::array<std::string_view, MAP_RESOURCE_COUNT> MAP_RESOURCE_NAMES = {
    "food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"};

struct MapTile {
    std::array<int, MAP_RESOURCE_COUNT> resources{};
    std::vector<std::uint32_t> player_ids;
};

class GameMap {
public:
    // Constructs a game map with the specified width and height.
    GameMap(int width, int height);

    int getWidth() const;
    int getHeight() const;

    // Accessors for individual tiles. Throws std::out_of_range if coordinates are invalid.
    MapTile& getTile(int x, int y);
    const MapTile& getTile(int x, int y) const;

    // modify tile content.
    // Absolute set (used by the bct protocol message, which carries full counts).
    void setResource(int x, int y, int resource_type_index, int count);
    void addResource(int x, int y, int resource_type_index, int amount);
    void removeResource(int x, int y, int resource_type_index, int amount);
    void addPlayerToTile(int x, int y, std::uint32_t player_id);
    void removePlayerFromTile(int x, int y, std::uint32_t player_id);

private:
    int _width;
    int _height;
    std::vector<MapTile> _tiles; // Stores tiles in row-major order.

    // Helper for bounds checking and index calculation.
    bool isValidCoordinate(int x, int y) const;
    size_t getIndex(int x, int y) const;
};