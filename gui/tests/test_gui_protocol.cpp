// Sanity check on the GUI protocol tag constants the parser will dispatch on.
// Migrate to Catch2 once vcpkg is wired (ADR-006).
#include "zappy/protocol/gui_protocol.hpp"

#include <cassert>
#include <iostream>

namespace gui = zappy::protocol::gui;

int main()
{
    // Tags are exactly 3 chars and stable on the wire.
    assert(gui::MAP_SIZE == "msz");
    assert(gui::TILE_CONTENT == "bct");
    assert(gui::PLAYER_NEW == "pnw");
    assert(gui::END_GAME == "seg");
    assert(gui::MAP_SIZE.size() == 3);

    assert(static_cast<int>(gui::Orientation::North) == 1);
    assert(static_cast<int>(gui::Orientation::West) == 4);

    std::cout << "gui protocol tags OK\n";
    return 0;
}
