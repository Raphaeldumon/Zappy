cmake_minimum_required(VERSION 3.24)

project(zappy
    VERSION 0.1.0
    DESCRIPTION "G-YEP-400 Zappy — server, GUI (Vulkan), AI (RL)"
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
option(ZAPPY_BUILD_SERVER     "Build zappy_server"                        ON)
option(ZAPPY_BUILD_AI         "Build zappy_ai (C++ client)"               ON)
option(ZAPPY_BUILD_GUI        "Build zappy_gui (Vulkan; auto-stub if deps missing)" ON)
option(ZAPPY_BUILD_TESTS      "Build unit tests"                          ON)
option(ZAPPY_BUILD_SIM        "Build pybind11 simulator (needs pybind11)" OFF)
option(ZAPPY_ENABLE_SANITIZERS "Enable ASan+UBSan (Debug)"                OFF)
option(ZAPPY_ENABLE_COVERAGE  "Enable gcov coverage flags"                OFF)

include(compile_options)
include(sanitizers)

if(ZAPPY_BUILD_TESTS)
    enable_testing()
endif()

# ---- Components -------------------------------------------------------------
add_subdirectory(protocol)

if(ZAPPY_BUILD_SERVER)
    add_subdirectory(server)
endif()
if(ZAPPY_BUILD_AI)
    add_subdirectory(ai_cpp)
endif()
if(ZAPPY_BUILD_GUI)
    add_subdirectory(gui)
endif()
if(ZAPPY_BUILD_SIM)
    add_subdirectory(sim_python)
endif()

# ---- Summary ----------------------------------------------------------------
message(STATUS "")
message(STATUS "==== Zappy ${PROJECT_VERSION} configuration ====")
message(STATUS "  Build type     : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler       : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Server         : ${ZAPPY_BUILD_SERVER}")
message(STATUS "  AI (C++)       : ${ZAPPY_BUILD_AI}")
message(STATUS "  GUI            : ${ZAPPY_BUILD_GUI}")
message(STATUS "  Sim (pybind11) : ${ZAPPY_BUILD_SIM}")
message(STATUS "  Tests          : ${ZAPPY_BUILD_TESTS}")
message(STATUS "  Sanitizers     : ${ZAPPY_ENABLE_SANITIZERS}")
message(STATUS "================================================")
message(STATUS "")
