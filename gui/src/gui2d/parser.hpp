#pragma once

#include "world.hpp"

#include <string_view>

namespace zappy::gui2d
{

// Applies one GUI-protocol line (no trailing '\n') to the World.
// Returns false on an unknown/malformed tag so the caller can log drift.
// Server emits player/egg ids as "#<n>" — we strip the '#' here.
bool apply_line(World &world, std::string_view line);

} // namespace zappy::gui2d
