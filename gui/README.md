# gui/ — `zappy_gui`

Owners: **P3 Sami** (Vulkan core) · **P4 Inès** (gameplay scene & UX).

A Vulkan 1.3 viewer that connects to the server as a `GRAPHIC` client and renders the
game live (and from `.zrec` replays).

## Stub vs real build

The repo always compiles. CMake auto-detects the Vulkan stack:

| Deps present | Result |
|---|---|
| Vulkan SDK 1.3 + glfw3 | real windowed renderer (`ZAPPY_GUI_HAS_VULKAN`) |
| missing | stub `zappy_gui` that parses `--help` and prints a banner |

```bash
make            # stub here until the SDK + glfw3 are installed
make gui_on     # forces -DZAPPY_BUILD_GUI=ON and builds the real renderer
./zappy_gui --help
```

Install (vcpkg `gui` feature in `vcpkg.json`): `vulkan-headers`, `vk-bootstrap`, `vma`,
`glfw3`, `glm`, `glslang`, `imgui`. Plus a system **Vulkan SDK 1.3** (LunarG).

## Layout

```
src/main.cpp                 CLI + dispatch (stub or real)
src/renderer/vulkan_app.cpp  real window/renderer (compiled only with the SDK)
include/zappy/gui/
  net/gui_client.hpp         GUI-protocol line parser (P4)
  scene/scene.hpp            renderer-facing world snapshot (P4)
  ui/hud.hpp                 ImGui HUD (P4)
shaders/triangle.{vert,frag} placeholder shaders (P3 replaces)
assets/                      LFS-tracked glTF/KTX2/audio
tests/                       protocol tag checks (always builds)
```

## Where to start (Sprint 1)

- **P3**: instance/device/swapchain via vk-bootstrap, dynamic rendering, triangle
  (D2) -> textured cube + VMA + shader hot-reload (D3) -> frame graph (D4).
- **P4**: ImGui integration over the window, free-orbit camera, then the
  `GuiClient` parser fed by a `.zrec` fixture.
