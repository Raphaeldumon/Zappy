#pragma once

// GUI <-> Server protocol contract (G-YEP-400_zappy_GUI_protocol.pdf).
// SACRED: any change here needs an ADR + the 3 leads' approval (see CODEOWNERS).
//
// See docs/01_architecture/06_protocols.md for the authoritative spec.

#include <string_view>

namespace zappy::protocol::gui {

// 3-letter message tags. Server->GUI unless noted; several are also GUI->Server requests.
inline constexpr std::string_view MAP_SIZE = "msz";         // msz X Y
inline constexpr std::string_view TILE_CONTENT = "bct";     // bct X Y q0..q6
inline constexpr std::string_view MAP_CONTENT = "mct";      // GUI->Server request
inline constexpr std::string_view TEAM_NAMES = "tna";       // tna N
inline constexpr std::string_view PLAYER_NEW = "pnw";       // pnw n X Y O L N
inline constexpr std::string_view PLAYER_POS = "ppo";       // ppo n X Y O
inline constexpr std::string_view PLAYER_LEVEL = "plv";     // plv n L
inline constexpr std::string_view PLAYER_INV = "pin";       // pin n X Y q0..q6
inline constexpr std::string_view PLAYER_EXPEL = "pex";     // pex n
inline constexpr std::string_view PLAYER_BROADCAST = "pbc"; // pbc n M
inline constexpr std::string_view INCANT_START = "pic";     // pic X Y L n n ...
inline constexpr std::string_view INCANT_END = "pie";       // pie X Y R
inline constexpr std::string_view PLAYER_FORK = "pfk";      // pfk n
inline constexpr std::string_view RESOURCE_DROP = "pdr";    // pdr n i
inline constexpr std::string_view RESOURCE_TAKE = "pgt";    // pgt n i
inline constexpr std::string_view PLAYER_DIE = "pdi";       // pdi n
inline constexpr std::string_view EGG_NEW = "enw";          // enw e n X Y
inline constexpr std::string_view EGG_HATCH = "ebo";        // ebo e
inline constexpr std::string_view EGG_DIE = "edi";          // edi e
inline constexpr std::string_view TIME_GET = "sgt";         // sgt T
inline constexpr std::string_view TIME_SET = "sst";         // sst T
inline constexpr std::string_view END_GAME = "seg";         // seg N
inline constexpr std::string_view SERVER_MSG = "smg";       // smg M
inline constexpr std::string_view UNKNOWN_CMD = "suc";      // suc
inline constexpr std::string_view BAD_PARAM = "sbp";        // sbp

// Player orientation as used on the wire.
enum class Orientation { North = 1, East = 2, South = 3, West = 4 };

} // namespace zappy::protocol::gui
