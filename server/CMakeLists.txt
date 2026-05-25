# Server: pure logic lib (libzappy_core) + the real runtime executable (zappy_server).
# The core/ split keeps game logic free of networking so the pybind11 simulator can
# reuse it (see sim_python/).

# ---- libzappy_core : pure game logic, no I/O --------------------------------
add_library(zappy_core STATIC
    core/world_state.cpp
    core/event_scheduler.cpp)
add_library(zappy::core ALIAS zappy_core)

target_include_directories(zappy_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zappy_core
    PUBLIC  zappy_protocol
    PRIVATE zappy_compile_options zappy_sanitizers)

# ---- zappy_server : runtime executable --------------------------------------
add_executable(zappy_server
    runtime/main.cpp
    runtime/args.cpp)

target_include_directories(zappy_server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zappy_server
    PRIVATE zappy_core zappy_protocol zappy_compile_options zappy_sanitizers)

# ---- tests ------------------------------------------------------------------
if(ZAPPY_BUILD_TESTS)
    add_executable(zappy_world_state_tests tests/test_world_state.cpp)
    target_link_libraries(zappy_world_state_tests
        PRIVATE zappy_core zappy_protocol zappy_compile_options zappy_test_options)
    add_test(NAME server_world_state COMMAND zappy_world_state_tests)
    set_tests_properties(server_world_state PROPERTIES LABELS "server;core;unit")

    add_executable(zappy_event_scheduler_tests tests/test_event_scheduler.cpp)
    target_link_libraries(zappy_event_scheduler_tests
        PRIVATE zappy_core zappy_protocol zappy_compile_options zappy_test_options)
    add_test(NAME server_event_scheduler COMMAND zappy_event_scheduler_tests)
    set_tests_properties(server_event_scheduler PROPERTIES LABELS "server;core;unit")
endif()
