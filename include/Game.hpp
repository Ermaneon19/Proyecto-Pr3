#pragma once

#include <SFML/Graphics.hpp>
#include <list.hpp>            // Contenedor de tu oleada
#include "Enemy.hpp"           // Tu clase

class Game {
private:
    sf::RenderWindow window;
    sf::Clock clock;

    // Tu área de responsabilidad: La lista de enemigos
    Designar::SLList<Enemy> swarm;

    // Métodos del bucle principal
    void processEvents();
    void update(float deltaTime);
    void render();

public:
    Game();
    void run();
};