# ai_cpp: the zappy_ai client executable.
# S1 scope: CLI + hello. Later it embeds a libtorch policy (ADR pending) and drives
# the AI protocol via a socket client. The policy is trained in ai_python/ and the
# checkpoint loaded from models/current/model.pt.

add_executable(zappy_ai
    src/main.cpp
    src/args.cpp)

target_include_directories(zappy_ai PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(zappy_ai
    PRIVATE zappy_protocol zappy_compile_options zappy_sanitizers)

if(ZAPPY_BUILD_TESTS)
    add_executable(zappy_ai_tests tests/test_args.cpp src/args.cpp)
    target_include_directories(zappy_ai_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(zappy_ai_tests
        PRIVATE zappy_protocol zappy_compile_options zappy_test_options)
    add_test(NAME ai_client COMMAND zappy_ai_tests)
    set_tests_properties(ai_client PROPERTIES LABELS "ai;unit")
endif()
