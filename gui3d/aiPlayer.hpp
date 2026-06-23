#pragma once

#include <cstdint>
#include <string>

enum class Orientation : std::uint8_t
{
    North = 1,
    East = 2,
    South = 3,
    West = 4
};

class aiPlayer
{
private:
    const std::uint32_t _id;
    const std::string _team;
    int x{};
    int y{};
    Orientation orientation{Orientation::North};
    int level{1};
    int life_units{};
    bool alive{true};
public:
    aiPlayer(std::uint32_t id, const std::string& team, int x, int y, Orientation orientation) : _id(id), _team(team), x(x), y(y), orientation(orientation) {};
    ~aiPlayer() = default;
    void changePosition(int newX, int newY) {
        x = newX;
        y = newY;
    };
    void changeOrientation(Orientation newOrientation) {
        orientation = newOrientation;
    };
    void changeLevel(int newLevel) {
        level = newLevel;
    };
    void changeLifeUnits(int newLifeUnits) {
        life_units = newLifeUnits;
        if (life_units <= 0) {
            alive = false;
        }
    };
    void setAlive(bool isAlive) {
        alive = isAlive;
    };
    void setLifeUnits(int newLifeUnits) {
        life_units = newLifeUnits;
    };
    void setLevel(int newLevel) {
        level = newLevel;
    };
    std::uint32_t getId() const {return _id;};
    std::string getTeam() const {return _team;};
    int getX() const {return x;};
    int getY() const {return y;};
    Orientation getOrientation() const {return orientation;};
    int getLevel() const {return level;};
    int getLifeUnits() const {return life_units;};
    bool isAlive() const {return alive;};
};
