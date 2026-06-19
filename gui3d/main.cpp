#include "interface.hpp"
#include <cstdlib>
#include <ctime>
#include <iostream>

int main()
{
    // Basic map and window dimensions for testing
    const int mapWidth = 20;
    const int mapHeight = 20;
    const int windowWidth = 1280;
    const int windowHeight = 720;

    std::cout << "Starting Zappy 3D GUI..." << std::endl;

    try {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));

        Interface app(mapWidth, mapHeight, windowWidth, windowHeight);
        for (int i = 0; i < 50; ++i) {
            int x = std::rand() % mapWidth;
            int y = std::rand() % mapHeight;
            int resourceType = std::rand() % MAP_RESOURCE_COUNT;
            int amount = (std::rand() % 5) + 1;
            app.getMap().addResource(x, y, resourceType, amount);
        }
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 84;
    }

    return 0;
}
