# zappy_gui — 3D graphical client (raylib).
#
# Built as part of the normal CMake tree (no separate Makefile), so object files
# land under build/ instead of cluttering this source dir. Output goes to
# build/bin/zappy_gui; the root Makefile copies it to ./zappy_gui.
#
# Kept off the strict zappy_compile_options: raylib's headers and the wrapper
# trip -Wconversion / -Wpedantic, which aren't worth policing on GUI glue code.

# Tests du module environnement (logique pure, aucune dépendance raylib).
add_executable(zappy_gui_environment_tests
    tests/test_environment.cpp
    environment.cpp)
set_target_properties(zappy_gui_environment_tests PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(zappy_gui_environment_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(zappy_gui_environment_tests PRIVATE -Wall -Wextra)
add_test(NAME gui_environment COMMAND zappy_gui_environment_tests)

# Read-only Zappy TV HTTP dashboard (pure sockets, no raylib dependency).
add_executable(zappy_gui_tv_tests
    tests/test_tv_server.cpp
    tvServer.cpp)
set_target_properties(zappy_gui_tv_tests PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(zappy_gui_tv_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(zappy_gui_tv_tests PRIVATE -Wall -Wextra)
target_link_libraries(zappy_gui_tv_tests PRIVATE pthread dl)
add_test(NAME gui_tv_dashboard COMMAND zappy_gui_tv_tests)

find_library(RAYLIB_LIBRARY NAMES raylib)

if(NOT RAYLIB_LIBRARY)
    message(WARNING
        "raylib not found — skipping zappy_gui. Install raylib (libraylib) to build the GUI.")
    return()
endif()

add_executable(zappy_gui
    main.cpp
    interface.cpp
    raylibWrapper.cpp
    gameMap.cpp
    netClient.cpp
    protocolParser.cpp
    environment.cpp
    particles.cpp
    groundCover.cpp
    tvServer.cpp)

set_target_properties(zappy_gui PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

target_include_directories(zappy_gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(zappy_gui PRIVATE -Wall -Wextra)
target_link_libraries(zappy_gui PRIVATE
    ${RAYLIB_LIBRARY} GL m pthread dl rt X11)
