# zappy_sim: a pybind11 module wrapping libzappy_core for fast headless training.
# Only configured when -DZAPPY_BUILD_SIM=ON (needs pybind11). It reuses the server's
# pure logic (zappy_core) so the simulator and the real server can never diverge.

find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(zappy_sim src/zappy_sim_pybind.cpp)
target_link_libraries(zappy_sim
    PRIVATE zappy_core zappy_protocol zappy_compile_options)

# Place the .so where Python can import it after build.
set_target_properties(zappy_sim PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
