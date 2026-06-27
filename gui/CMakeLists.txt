# zappy_gui — 3D graphical client (raylib).
#
# Built as part of the normal CMake tree (no separate Makefile), so object files
# land under build/ instead of cluttering this source dir. Output goes to
# build/bin/zappy_gui; the root Makefile copies it to ./zappy_gui.
#
# Kept off the strict zappy_compile_options: raylib's headers and the wrapper
# trip -Wconversion / -Wpedantic, which aren't worth policing on GUI glue code.

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
    protocolParser.cpp)

set_target_properties(zappy_gui PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

target_include_directories(zappy_gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(zappy_gui PRIVATE -Wall -Wextra)
target_link_libraries(zappy_gui PRIVATE
    ${RAYLIB_LIBRARY} GL m pthread dl rt X11)
