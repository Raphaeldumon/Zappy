// Real Vulkan renderer entry. Compiled ONLY when Vulkan + glfw3 are found
// (ZAPPY_GUI_HAS_VULKAN). This is P3's starting point: open a window, then build the
// instance/device/swapchain (vk-bootstrap), then the triangle (S1 D2).
#if defined(ZAPPY_GUI_HAS_VULKAN)

#define GLFW_INCLUDE_VULKAN
#include <cstdlib>
#include <GLFW/glfw3.h>
#include <iostream>

namespace zappy::gui {

int run_vulkan_app() {
    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "zappy_gui: glfwInit failed\n";
        return 84;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Zappy", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "zappy_gui: window creation failed\n";
        glfwTerminate();
        return 84;
    }

    // TODO(P3): vk-bootstrap instance/device/queue, swapchain, dynamic rendering,
    //           then the triangle sample (Sascha Willems 01_triangle).
    std::cout << "zappy_gui: window open (ESC to quit)\n";
    while (glfwWindowShouldClose(window) == 0) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace zappy::gui

#endif // ZAPPY_GUI_HAS_VULKAN
