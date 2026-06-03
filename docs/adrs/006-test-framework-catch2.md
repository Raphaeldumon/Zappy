# ADR-006 — C++ test framework: Catch2 (via vcpkg)

- **Status**: Proposed
- **Date**: 2026-05-25
- **Deciders**: P1 Léa, P6 Yanis
- **Touches protocol contract?**: no

## Context

The S1 base ships unit tests as plain `assert`-based executables registered with CTest
so the repo compiles and tests run with **zero external dependencies** before vcpkg is
set up. That's good enough to prove the build, but we want fixtures, sections,
parameterized cases and rich failure output for real coverage work.

## Decision

Adopt **Catch2 v3** pulled via vcpkg. Once `vcpkg.json` is wired into the CMake
toolchain, migrate the `tests/test_*.cpp` files from raw asserts to `TEST_CASE`s and
link `Catch2::Catch2WithMain`.

## Alternatives considered

- **GoogleTest** — heavier, more boilerplate; fine but Catch2 reads cleaner for us.
- **doctest** — lightest, but Catch2's ecosystem (matchers, generators) wins for game
  logic tests.

## Consequences

- Until migration, keep new tests in the simple `int main()` + `assert` style so CI
  stays green without vcpkg.
- After migration, `make tests_run` and CTest labels (`server`, `gui`, `ai`,
  `protocol`, `unit`) stay unchanged — only the test bodies change.
