# Optional ASan + UBSan, gated behind ZAPPY_ENABLE_SANITIZERS.
# Exposed as INTERFACE target zappy_sanitizers (a no-op when disabled).

add_library(zappy_sanitizers INTERFACE)

if(ZAPPY_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zappy_sanitizers INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer)
    target_link_options(zappy_sanitizers INTERFACE
        -fsanitize=address,undefined)
    message(STATUS "Sanitizers enabled: address, undefined")
endif()
