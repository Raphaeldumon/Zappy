# Server: pure logic lib (libzappy_core) + the real runtime executable (zappy_server).
# The core/ split keeps game logic free of networking so it stays unit-testable
# (and reusable by a future simulator binding).

# ---- libzappy_core : pure game logic, no I/O --------------------------------
add_library(zappy_core STATIC
    core/world_state.cpp
    core/event_scheduler.cpp)
add_library(zappy::core ALIAS zappy_core)

target_include_directories(zappy_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zappy_core
    PUBLIC  zappy_protocol
    PRIVATE zappy_compile_options zappy_sanitizers)

# ---- libzappy_server_net : network I/O layer --------------------------------
add_library(zappy_server_net STATIC
    net/client.cpp
    net/network_layer.cpp)

target_include_directories(zappy_server_net PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zappy_server_net
    PUBLIC  zappy_core
    PRIVATE zappy_compile_options zappy_sanitizers)

# ---- libzappy_server_protocol : wire format encode/decode -------------------
add_library(zappy_server_protocol STATIC
    protocol/handshake.cpp
    protocol/ai_handler.cpp
    protocol/gui_emitter.cpp
    protocol/gui_handler.cpp)

target_include_directories(zappy_server_protocol PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zappy_server_protocol
    PUBLIC  zappy_core
    PRIVATE zappy_compile_options zappy_sanitizers)

# ---- zappy_server : runtime executable --------------------------------------
add_executable(zappy_server
    runtime/server.cpp
    runtime/server_commands.cpp
    runtime/parse_args.cpp
    runtime/main.cpp)

target_include_directories(zappy_server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zappy_server
    PRIVATE zappy_server_net zappy_server_protocol zappy_core
            zappy_protocol zappy_compile_options zappy_sanitizers)

# ---- tests ------------------------------------------------------------------
if(ZAPPY_BUILD_TESTS)
    add_executable(zappy_world_state_tests tests/test_world_state.cpp)
    target_include_directories(zappy_world_state_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(zappy_world_state_tests
        PRIVATE zappy_core zappy_protocol zappy_compile_options zappy_test_options)
    add_test(NAME server_world_state COMMAND zappy_world_state_tests)
    set_tests_properties(server_world_state PROPERTIES LABELS "server;core;unit")

    add_executable(zappy_event_scheduler_tests tests/test_event_scheduler.cpp)
    target_include_directories(zappy_event_scheduler_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(zappy_event_scheduler_tests
        PRIVATE zappy_core zappy_protocol zappy_compile_options zappy_test_options)
    add_test(NAME server_event_scheduler COMMAND zappy_event_scheduler_tests)
    set_tests_properties(server_event_scheduler PROPERTIES LABELS "server;core;unit")

    add_executable(zappy_game_rules_tests tests/test_game_rules.cpp)
    target_include_directories(zappy_game_rules_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(zappy_game_rules_tests
        PRIVATE zappy_core zappy_protocol zappy_compile_options zappy_test_options)
    add_test(NAME server_game_rules COMMAND zappy_game_rules_tests)
    set_tests_properties(server_game_rules PROPERTIES LABELS "server;core;unit")

    add_executable(zappy_protocol_ai_tests tests/test_protocol_ai.cpp)
    target_include_directories(zappy_protocol_ai_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(zappy_protocol_ai_tests
        PRIVATE zappy_server_protocol zappy_core zappy_protocol
                zappy_compile_options zappy_test_options)
    add_test(NAME server_protocol_ai COMMAND zappy_protocol_ai_tests)
    set_tests_properties(server_protocol_ai PROPERTIES LABELS "server;protocol;unit")

    add_executable(zappy_protocol_gui_tests tests/test_protocol_gui.cpp)
    target_include_directories(zappy_protocol_gui_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(zappy_protocol_gui_tests
        PRIVATE zappy_server_protocol zappy_core zappy_protocol
                zappy_compile_options zappy_test_options)
    add_test(NAME server_protocol_gui COMMAND zappy_protocol_gui_tests)
    set_tests_properties(server_protocol_gui PROPERTIES LABELS "server;protocol;unit")

    add_executable(zappy_handshake_tests tests/test_handshake.cpp)
    target_include_directories(zappy_handshake_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(zappy_handshake_tests
        PRIVATE zappy_server_protocol zappy_core zappy_protocol
                zappy_compile_options zappy_test_options)
    add_test(NAME server_handshake COMMAND zappy_handshake_tests)
    set_tests_properties(server_handshake PROPERTIES LABELS "server;protocol;unit")

    add_executable(zappy_network_tests tests/test_network.cpp)
    target_include_directories(zappy_network_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(zappy_network_tests
        PRIVATE zappy_server_net zappy_core zappy_protocol
                zappy_compile_options zappy_test_options)
    add_test(NAME server_network COMMAND zappy_network_tests)
    set_tests_properties(server_network PROPERTIES LABELS "server;net;unit")

    # parse_args has no deps beyond the stdlib, so compile it straight in.
    add_executable(zappy_parse_args_tests tests/test_parse_args.cpp runtime/parse_args.cpp)
    target_include_directories(zappy_parse_args_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(zappy_parse_args_tests
        PRIVATE zappy_compile_options zappy_test_options)
    add_test(NAME server_parse_args COMMAND zappy_parse_args_tests)
    set_tests_properties(server_parse_args PROPERTIES LABELS "server;runtime;unit")
endif()
