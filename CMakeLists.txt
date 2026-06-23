cmake_minimum_required(VERSION 3.22)

project(zappy
    VERSION 0.1.0
    DESCRIPTION "G-YEP-400 Zappy — server (C++20) + AI training (Python, ai/)"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---- Global C++ settings ----------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# All executables land in build/bin; the Makefile copies them to repo root.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ---- Options ----------------------------------------------------------------
option(ZAPPY_BUILD_TESTS       "Build unit tests"           ON)
option(ZAPPY_ENABLE_SANITIZERS "Enable ASan+UBSan (Debug)"  OFF)
option(ZAPPY_ENABLE_COVERAGE   "Enable gcov coverage flags" OFF)

include(compile_options)
include(sanitizers)

if(ZAPPY_BUILD_TESTS)
    enable_testing()
endif()

# ---- Components ---------------------------------------------------------------
# C++ lives in protocol/ (shared wire contract) and server/. The AI is pure
# Python (ai/, no CMake); the GUI was removed from this branch.
add_subdirectory(protocol)
add_subdirectory(server)

# ---- Integration tests (AI <-> server <-> GUI over real sockets) -------------
# Driven by tools/run_integration.py against the built zappy_server. Tagged
# "integration" so unit-only runs can skip it: ctest -LE integration
if(ZAPPY_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME integration_protocol
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/run_integration.py
                    --server $<TARGET_FILE:zappy_server>
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        set_tests_properties(integration_protocol PROPERTIES LABELS "integration" TIMEOUT 120)
    endif()
endif()

# ---- Summary ----------------------------------------------------------------
message(STATUS "")
message(STATUS "==== Zappy ${PROJECT_VERSION} configuration ====")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler   : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Tests      : ${ZAPPY_BUILD_TESTS}")
message(STATUS "  Sanitizers : ${ZAPPY_ENABLE_SANITIZERS}")
message(STATUS "  Coverage   : ${ZAPPY_ENABLE_COVERAGE}")
message(STATUS "================================================")
message(STATUS "")
