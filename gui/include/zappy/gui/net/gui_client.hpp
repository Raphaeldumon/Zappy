#pragma once

#include "zappy/protocol/gui_protocol.hpp"

#include <string>
#include <string_view>

namespace zappy::gui::net
{

// Parses GUI-protocol lines coming from the server and turns them into scene
// updates. Line-based, each message terminated by '\n'. P4 implements this in S1;
// it must also work offline by replaying a `.zrec` dump.
class GuiClient
{
  public:
    // Feed one complete protocol line (without the trailing '\n'). Returns false if
    // the tag is unknown so callers can flag protocol drift early.
    bool handle_line(std::string_view line);

    // TODO(P4): connect(host, port) sending "GRAPHIC\n", then read loop.
    // TODO(P4): dispatch table tag(3 chars) -> handler updating the Scene.
};

} // namespace zappy::gui::net
