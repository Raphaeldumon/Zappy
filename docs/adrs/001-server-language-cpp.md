# ADR-001 — Server & GUI language: C++20

- **Status**: Accepted
- **Date**: 2026-05-25
- **Deciders**: P1 Léa, P2 Marc, P3 Sami
- **Touches protocol contract?**: no

## Context

The subject mandates C++ for the server and GUI. We need a standard that gives us
`std::optional`, designated initializers, concepts and `constexpr` containers without
relying on bleeding-edge compiler support, since CI runs gcc-13 and clang.

## Decision

Target **C++20** across all C++ components (`server`, `gui`, `ai_cpp`, `sim_python`,
`protocol`). Enforced in CMake via `CMAKE_CXX_STANDARD 20` with extensions off.

## Alternatives considered

- **C++17** — safest portability, but loses concepts / `constexpr std::array` ergonomics.
- **C++23** — modules/`std::expected` are tempting but compiler support is uneven on
  the CI images; revisit if we upgrade toolchains.

## Consequences

- One standard everywhere; no per-module drift.
- `protocol/` uses `constexpr std::string_view` / `std::array` freely.
- If a CI compiler lacks a C++20 feature we use, we pin the toolchain rather than
  downgrade the standard.
