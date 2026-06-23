#include "gameMap.hpp"
#include <algorithm> // For std::remove
#include <stdexcept> // For std::invalid_argument, std::out_of_range

GameMap::GameMap(int width, int height) : _width(width), _height(height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Map width and height must be positive.");
    }
    _tiles.resize(static_cast<size_t>(width) * height);
}

int GameMap::getWidth() const {
    return _width;
}

int GameMap::getHeight() const {
    return _height;
}

bool GameMap::isValidCoordinate(int x, int y) const {
    return x >= 0 && x < _width && y >= 0 && y < _height;
}

size_t GameMap::getIndex(int x, int y) const {
    return static_cast<size_t>(y) * _width + x;
}

MapTile& GameMap::getTile(int x, int y) {
    if (!isValidCoordinate(x, y)) {
        throw std::out_of_range("Tile coordinates (" + std::to_string(x) + ", " + std::to_string(y) +
                                ") are out of map bounds [0," + std::to_string(_width - 1) + "],[0," +
                                std::to_string(_height - 1) + "].");
    }
    return _tiles[getIndex(x, y)];
}

const MapTile& GameMap::getTile(int x, int y) const {
    if (!isValidCoordinate(x, y)) {
        throw std::out_of_range("Tile coordinates (" + std::to_string(x) + ", " + std::to_string(y) +
                                ") are out of map bounds [0," + std::to_string(_width - 1) + "],[0," +
                                std::to_string(_height - 1) + "].");
    }
    return _tiles[getIndex(x, y)];
}

void GameMap::setResource(int x, int y, int resource_type_index, int count) {
    MapTile& tile = getTile(x, y); // Will throw if invalid coords
    if (resource_type_index < 0 || resource_type_index >= MAP_RESOURCE_COUNT) {
        throw std::invalid_argument("Invalid resource type index.");
    }
    tile.resources[resource_type_index] = std::max(0, count);
}

void GameMap::addResource(int x, int y, int resource_type_index, int amount) {
    MapTile& tile = getTile(x, y); // Will throw if invalid coords
    if (resource_type_index < 0 || resource_type_index >= MAP_RESOURCE_COUNT) {
        throw std::invalid_argument("Invalid resource type index.");
    }
    tile.resources[resource_type_index] += amount;
}

void GameMap::removeResource(int x, int y, int resource_type_index, int amount) {
    MapTile& tile = getTile(x, y); // Will throw if invalid coords
    if (resource_type_index < 0 || resource_type_index >= MAP_RESOURCE_COUNT) {
        throw std::invalid_argument("Invalid resource type index.");
    }
    tile.resources[resource_type_index] = std::max(0, tile.resources[resource_type_index] - amount);
}

void GameMap::addPlayerToTile(int x, int y, std::uint32_t player_id) {
    MapTile& tile = getTile(x, y); // Will throw if invalid coords
    if (std::find(tile.player_ids.begin(), tile.player_ids.end(), player_id) == tile.player_ids.end()) {
        tile.player_ids.push_back(player_id);
    }
}

void GameMap::removePlayerFromTile(int x, int y, std::uint32_t player_id) {
    MapTile& tile = getTile(x, y); // Will throw if invalid coords
    tile.player_ids.erase(std::remove(tile.player_ids.begin(), tile.player_ids.end(), player_id), tile.player_ids.end());
}
