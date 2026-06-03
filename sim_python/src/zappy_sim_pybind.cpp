// pybind11 bindings exposing the C++ game logic to Python training (P6; ADR-003).
//
// S1 stub: a Sim class backed by a real WorldState. reset()/step() return placeholder
// state of the right shape so ai_python can integrate before the full step() exists.

#include "core/world_state.hpp"

#include <cstdint>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <random>
#include <vector>

namespace py = pybind11;

namespace
{

class Sim
{
  public:
    Sim(int width = 10, int height = 10, int n_agents = 4) : world_(width, height), n_agents_(n_agents)
    {
    }

    // Returns a flat observation per agent (placeholder values for now).
    std::vector<std::vector<double>> reset(std::uint64_t seed = 0)
    {
        rng_.seed(seed);
        step_ = 0;
        return observations();
    }

    // TODO(P6): apply `actions` to world_ via libzappy_core, advance the scheduler.
    std::vector<std::vector<double>> step(const std::vector<int> & /*actions*/)
    {
        ++step_;
        return observations();
    }

    int width() const
    {
        return world_.width();
    }
    int height() const
    {
        return world_.height();
    }
    int n_agents() const
    {
        return n_agents_;
    }

  private:
    std::vector<std::vector<double>> observations()
    {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        std::vector<std::vector<double>> obs(static_cast<std::size_t>(n_agents_));
        for (auto &row : obs)
        {
            row.resize(OBS_DIM);
            for (auto &v : row)
            {
                v = dist(rng_);
            }
        }
        return obs;
    }

    static constexpr std::size_t OBS_DIM = 64; // keep in sync with encoding.OBS_DIM

    zappy::core::WorldState world_;
    int n_agents_;
    std::uint64_t step_{0};
    std::mt19937_64 rng_{};
};

} // namespace

PYBIND11_MODULE(zappy_sim, m)
{
    m.doc() = "Zappy headless simulator (pybind11 over libzappy_core)";
    py::class_<Sim>(m, "Sim")
        .def(py::init<int, int, int>(), py::arg("width") = 10, py::arg("height") = 10, py::arg("n_agents") = 4)
        .def("reset", &Sim::reset, py::arg("seed") = 0)
        .def("step", &Sim::step, py::arg("actions"))
        .def_property_readonly("width", &Sim::width)
        .def_property_readonly("height", &Sim::height)
        .def_property_readonly("n_agents", &Sim::n_agents);
}
