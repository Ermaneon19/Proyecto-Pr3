#pragma once

#include "Enemy.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Time.hpp>

class Game {
public:
    Game();
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void run();

private:
    void processEvents();
    void update(sf::Time deltaTime);
    void render();

    sf::RenderWindow window_;
    Enemy enemy_{100.0f, 100.0f};
    bool imguiInitialized_{false};
};