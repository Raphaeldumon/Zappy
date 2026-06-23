#pragma once

namespace zappy::runtime
{

// Server capacity limits. These guard against absurd input and file-descriptor
// exhaustion (each client is one fd; the OS soft limit is typically 1024).

inline constexpr int MAX_CLIENTS_PER_TEAM = 100; // ceiling on -c (realistic play is far lower)
inline constexpr int MAX_TEAMS = 8;              // ceiling on team count (-n)
inline constexpr int MAX_GUI_CLIENTS = 6;        // concurrent GRAPHIC connections

inline constexpr int MAX_TOTAL_CLIENTS = MAX_CLIENTS_PER_TEAM * MAX_TEAMS + MAX_GUI_CLIENTS;

} // namespace zappy::runtime
