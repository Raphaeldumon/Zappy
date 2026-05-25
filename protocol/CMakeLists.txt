# zappy_protocol — header-only contract shared by server, gui and ai.
# Changing anything here requires an ADR + approval of the 3 leads (see CODEOWNERS).

add_library(zappy_protocol INTERFACE)
add_library(zappy::protocol ALIAS zappy_protocol)

target_include_directories(zappy_protocol INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(zappy_protocol INTERFACE cxx_std_20)

if(ZAPPY_BUILD_TESTS)
    add_executable(zappy_protocol_tests tests/test_protocol.cpp)
    target_link_libraries(zappy_protocol_tests
        PRIVATE zappy_protocol zappy_compile_options zappy_test_options)
    add_test(NAME protocol_contract COMMAND zappy_protocol_tests)
    set_tests_properties(protocol_contract PROPERTIES LABELS "protocol;unit")
endif()
