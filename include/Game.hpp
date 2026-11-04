#pragma once

#include <SFML/Graphics.hpp>
#include <list.hpp>
#include "Enemy.hpp"
#include "Core.hpp" 

class Game {
private:
    sf::RenderWindow window;
    sf::Clock clock;

    // Variables para cargar la imagen y textura del insecto una sola vez
    sf::Image enemyImage;
    sf::Texture enemyTexture;

    Designar::SLList<Enemy> swarm;
    Designar::SLList<Core> cores; 

    int score = 0; 

    void processEvents();
    void update(float deltaTime);
    void render();

public:
    Game();
    void run();
};