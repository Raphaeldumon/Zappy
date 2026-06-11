#include "interface.hpp"
#include <iostream>

int main()
{
    // Basic map and window dimensions for testing
    const int mapWidth = 20;
    const int mapHeight = 20;
    const int windowWidth = 1280;
    const int windowHeight = 720;

    std::cout << "Starting Zappy 3D GUI..." << std::endl;

    Interface app(mapWidth, mapHeight, windowWidth, windowHeight);
    app.getMap().addResource(0, 0, 0, 10); // Example: add some food to the map
    for (int i = 0; i < 50; ++i) {
        int x = rand() % mapWidth;
        int y = rand() % mapHeight;
        int resourceType = rand() % MAP_RESOURCE_COUNT;
        int amount = (rand() % 5) + 1;
        app.getMap().addResource(x, y, resourceType, amount);
    }
    app.run();

    return 0;
}