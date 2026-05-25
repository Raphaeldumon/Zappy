# 03 — Références

## Sujet Epitech (PDFs fournis)

- `G-YEP-400_zappy.pdf` — Sujet principal (15 pages, v3.2.4)
- `G-YEP-400_zappy_GUI_protocol.pdf` — Protocole GUI (3 pages, v3.3)
- `G-YEP-400_kickoff.pdf` — Slides kickoff Epitech (10 pages)
- `zappy_ref-v3.0.1.tgz` — Tarball serveur de référence (utilisé pour comparaison)

Conserver les PDFs en local mais **ne pas commit** dans le repo (copyright Epitech).

## Vulkan

### Documentation officielle
- [Vulkan 1.3 Specification](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/)
- [Vulkan SDK LunarG](https://vulkan.lunarg.com/)
- [GLSL specification](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.pdf)

### Tutoriels recommandés
- [vkguide.dev](https://vkguide.dev/) — best modern Vulkan guide (Vulkan 1.3, dynamic rendering, bindless)
- [Vulkan Tutorial](https://vulkan-tutorial.com/) — classique
- [Sascha Willems Vulkan Samples](https://github.com/SaschaWillems/Vulkan) — référence pour features
- [LearnVulkan.com](https://www.learnvulkan.com/) — récents articles ray tracing, mesh shaders

### Ray tracing
- [Khronos RT extension spec](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_ray_tracing_pipeline.html)
- [NVIDIA Vulkan RT tutorial](https://developer.nvidia.com/rtx/raytracing/vkray)

### Performance / debug
- [RenderDoc](https://renderdoc.org/) — frame analysis tool
- [Tracy Profiler](https://github.com/wolfpld/tracy)
- [Nsight Graphics](https://developer.nvidia.com/nsight-graphics) (NVIDIA)
- [GPU Open AGS / RGA](https://gpuopen.com/) (AMD)

### Libs
- [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap)
- [VulkanMemoryAllocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [glslang](https://github.com/KhronosGroup/glslang) — runtime shader compilation
- [Dear ImGui](https://github.com/ocornut/imgui) — branche `docking`

## C++ / Réseau

- [asio standalone](https://think-async.com/Asio/)
- [Boost.Asio docs](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html) (compatible)
- [poll(2) man](https://man7.org/linux/man-pages/man2/poll.2.html)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)

## RL / Training

- [PyTorch docs](https://pytorch.org/docs/stable/)
- [TorchScript guide](https://pytorch.org/docs/stable/jit.html)
- [libtorch C++ tutorial](https://pytorch.org/cppdocs/)
- [RLlib (Ray)](https://docs.ray.io/en/latest/rllib/)
- [PettingZoo Multi-Agent](https://pettingzoo.farama.org/)
- [Gymnasium](https://gymnasium.farama.org/)
- [Stable-Baselines3](https://stable-baselines3.readthedocs.io/) (alternative considérée)
- [Spinning Up in DeepRL](https://spinningup.openai.com/) — pédagogique
- [CleanRL](https://github.com/vwxyzjn/cleanrl) — implémentations RL minimales

### Papers
- *Proximal Policy Optimization Algorithms* (Schulman et al., 2017)
- *MAPPO: The Surprising Effectiveness of PPO in Cooperative, Multi-Agent Games* (Yu et al., 2021)
- *Reward is enough* (Silver et al., 2021)

## DevOps / Outils

- [pre-commit](https://pre-commit.com/)
- [commitlint](https://commitlint.js.org/)
- [Conventional Commits](https://www.conventionalcommits.org/)
- [GitHub Actions docs](https://docs.github.com/en/actions)
- [CMake docs](https://cmake.org/cmake/help/latest/)
- [vcpkg](https://vcpkg.io/)
- [MkDocs Material](https://squidfunk.github.io/mkdocs-material/)
- [Doxygen](https://www.doxygen.nl/)

## Observabilité

- [Prometheus docs](https://prometheus.io/docs/)
- [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp)
- [Grafana docs](https://grafana.com/docs/grafana/latest/)

## Pybind11

- [pybind11 docs](https://pybind11.readthedocs.io/)
- [pybind11 GIL handling](https://pybind11.readthedocs.io/en/stable/advanced/misc.html#global-interpreter-lock-gil)

## Audio

- [miniaudio docs](https://miniaud.io/)
- [Audacity](https://www.audacityteam.org/) — édition .wav/.ogg

## Assets libres

- [Kenney.nl](https://kenney.nl/) — CC0 game assets
- [Quaternius](https://quaternius.com/) — CC0 low-poly models
- [Polyhaven](https://polyhaven.com/) — CC0 HDRIs, textures, 3D models
- [Sketchfab](https://sketchfab.com/) — filtrer CC licenses
- [freesound.org](https://freesound.org/) — SFX
- [incompetech.com](https://incompetech.com/) — musique royalty-free
- [Pixabay Music](https://pixabay.com/music/) — royalty-free

## Inspiration jeu / look

- *Cities: Skylines* — vue stratégique / particles
- *Frostpunk* — atmosphère sombre
- *No Man's Sky* — planètes procedurales
- *Spore* — élévation civilisation
- *Eufloria* — minimalisme tactique

## Liens internes au projet

- Site MkDocs déployé : `https://<org>.github.io/G-YEP-400-RUN-4-1-zappy-3/`
- Repo GitHub : `https://github.com/<org>/G-YEP-400-RUN-4-1-zappy-3`
- W&B project : `https://wandb.ai/<entity>/zappy-rl`
- Discord serveur équipe : `<lien>`
- Drive partagé (slides, vidéos, captures) : `<lien>`

## Communauté

- [Vulkan Discord](https://discord.gg/vulkan)
- [Khronos Slack](https://khr.io/slack)
- [Ray Slack](https://ray-distributed.slack.com)
- [r/vulkan](https://reddit.com/r/vulkan)
- [r/reinforcementlearning](https://reddit.com/r/reinforcementlearning)
