#pragma once

#include <string>

class GameMap;
struct GuiState;

// Applies one GUI-protocol line to the world model. Stateless: all mutated
// state lives in GameMap (tiles + player-id mirror) and GuiState (players,
// eggs, teams, winner). Unknown or cosmetic-only tags are ignored.
class ProtocolParser
{
  public:
    void apply(const std::string &line, GameMap &map, GuiState &state);
};
