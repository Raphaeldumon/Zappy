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
    ~aiPlayer();
};
