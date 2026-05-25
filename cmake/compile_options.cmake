# Shared compile options as an INTERFACE target. Link it into every Zappy target:
#   target_link_libraries(<tgt> PRIVATE zappy_compile_options)

add_library(zappy_compile_options INTERFACE)
target_compile_features(zappy_compile_options INTERFACE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zappy_compile_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wcast-align
        -Wunused
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Release>:-O2>)
elseif(MSVC)
    target_compile_options(zappy_compile_options INTERFACE /W4 /permissive-)
endif()

if(ZAPPY_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zappy_compile_options INTERFACE --coverage -O0 -g)
    target_link_options(zappy_compile_options INTERFACE --coverage)
endif()

# Test executables link this so assert() stays live even in Release builds (where
# CMake otherwise defines NDEBUG and turns every assert into a no-op).
add_library(zappy_test_options INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zappy_test_options INTERFACE -UNDEBUG)
endif()
