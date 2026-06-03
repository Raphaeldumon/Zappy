# gui: the zappy_gui executable.
#
# The repo must always compile, but the Vulkan stack (glfw3 + Vulkan SDK) isn't on
# every machine yet. So we DETECT the deps:
#   - found    -> build the real windowed renderer (ZAPPY_GUI_HAS_VULKAN defined)
#   - missing  -> build a stub that still parses --help and prints a banner
# Either way `zappy_gui` exists. Install the SDK + glfw3 (vcpkg) then reconfigure to
# get the real renderer; see gui/README.md.

find_package(Vulkan QUIET)

find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLFW QUIET glfw3)
    endif()
endif()

set(_gui_have_glfw OFF)
if(glfw3_FOUND OR GLFW_FOUND)
    set(_gui_have_glfw ON)
endif()

add_executable(zappy_gui src/main.cpp)
target_include_directories(zappy_gui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(zappy_gui
    PRIVATE zappy_protocol zappy_compile_options zappy_sanitizers)

if(Vulkan_FOUND AND _gui_have_glfw)
    message(STATUS "  gui: building REAL Vulkan renderer")
    target_sources(zappy_gui PRIVATE src/renderer/vulkan_app.cpp)
    target_compile_definitions(zappy_gui PRIVATE ZAPPY_GUI_HAS_VULKAN=1)
    target_link_libraries(zappy_gui PRIVATE Vulkan::Vulkan)
    if(glfw3_FOUND)
        target_link_libraries(zappy_gui PRIVATE glfw)
    else()
        target_include_directories(zappy_gui PRIVATE ${GLFW_INCLUDE_DIRS})
        target_link_libraries(zappy_gui PRIVATE ${GLFW_LIBRARIES})
    endif()
else()
    message(STATUS "  gui: Vulkan/glfw3 not found -> building STUB zappy_gui")
endif()

# ---- zappy_gui2d : throwaway raylib 2D debug viewer -------------------------
# Standalone from the Vulkan renderer above. Built only when raylib is found so
# the repo still configures on machines without it. Connects as a GRAPHIC client
# and draws whatever the server emits — meant for eyeballing server state, not
# the final GUI.
find_package(raylib QUIET)
if(NOT raylib_FOUND)
    # raylib installed via `make install` (no CMake config): probe the static lib.
    find_library(RAYLIB_LIB raylib)
    find_path(RAYLIB_INCLUDE raylib.h)
endif()

if(raylib_FOUND OR (RAYLIB_LIB AND RAYLIB_INCLUDE))
    message(STATUS "  gui: building zappy_gui2d (raylib debug viewer)")
    add_executable(zappy_gui2d
        src/gui2d/main.cpp
        src/gui2d/parser.cpp
        src/gui2d/net.cpp)
    target_include_directories(zappy_gui2d PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/gui2d")
    target_link_libraries(zappy_gui2d PRIVATE zappy_compile_options)
    if(raylib_FOUND)
        target_link_libraries(zappy_gui2d PRIVATE raylib)
    else()
        target_include_directories(zappy_gui2d PRIVATE "${RAYLIB_INCLUDE}")
        target_link_libraries(zappy_gui2d PRIVATE "${RAYLIB_LIB}")
    endif()
    # raylib's static build needs the system GL/X11/threading deps.
    find_package(OpenGL QUIET)
    if(OpenGL_FOUND)
        target_link_libraries(zappy_gui2d PRIVATE OpenGL::GL)
    endif()
    target_link_libraries(zappy_gui2d PRIVATE m pthread dl rt X11)
else()
    message(STATUS "  gui: raylib not found -> skipping zappy_gui2d")
endif()

if(ZAPPY_BUILD_TESTS)
    add_executable(zappy_gui_tests tests/test_gui_protocol.cpp)
    target_include_directories(zappy_gui_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(zappy_gui_tests
        PRIVATE zappy_protocol zappy_compile_options zappy_test_options)
    add_test(NAME gui_protocol COMMAND zappy_gui_tests)
    set_tests_properties(gui_protocol PROPERTIES LABELS "gui;unit")
endif()
