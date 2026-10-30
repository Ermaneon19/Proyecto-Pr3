#include "Game.hpp"
#include <SFML/Window/Mouse.hpp>
#include <cstdlib>
#include <ctime>

// 1. Constructor: Configura la ventana y genera los enemigos de prueba
Game::Game() : window(sf::VideoMode(800, 600), "Código Crítico - Prueba Persona 3") {
    std::srand(static_cast<unsigned int>(std::time(nullptr))); // Semilla para valores aleatorios

    // Aparecen 5 enemigos en posiciones aleatorias de la pantalla
    for (int i = 0; i < 5; ++i) {
        swarm.append(Enemy(std::rand() % 800, std::rand() % 600));
    }
}

// 2. Bucle principal del juego
void Game::run() {
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();

        processEvents();
        update(deltaTime);
        render();
    }
}

// 3. Procesar eventos (cerrar la ventana, etc.)
void Game::processEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window.close();
        }
    }
}

// 4. Actualizar lógica matemática
void Game::update(float deltaTime) {
    // Capturar la posición actual del ratón como "objetivo"
    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    float targetX = static_cast<float>(mousePos.x);
    float targetY = static_cast<float>(mousePos.y);

    // Iterar tu lista de DeSiGNAR para mover a cada enemigo
    auto it = swarm.begin();
    while (it != swarm.end()) {
        it->update(targetX, targetY, deltaTime);
        ++it;
    }
}

// 5. Renderizar gráficos
void Game::render() {
    window.clear(); // Limpiar el frame anterior (fondo negro)

    // Iterar tu lista de DeSiGNAR para dibujar cada enemigo
    auto it = swarm.begin();
    while (it != swarm.end()) {
        it->draw(window);
        ++it;
    }

    window.display(); // Mostrar el nuevo frame en pantalla
}