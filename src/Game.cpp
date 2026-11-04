#include "Game.hpp"
#include <SFML/Window/Mouse.hpp>
#include <cstdlib>
#include <ctime>

Game::Game() : window(sf::VideoMode(800, 600), "Código Crítico - Prueba Persona 3") {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    for (int i = 0; i < 5; ++i) {
        swarm.append(Enemy(std::rand() % 800, std::rand() % 600));
    }
}

void Game::run() {
    while (window.isOpen()) {
        const float deltaTime = clock.restart().asSeconds();
        processEvents();
        update(deltaTime);
        render();
    }
}

void Game::processEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window.close();
        }
    }
}

void Game::update(float deltaTime) {
    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    float targetX = static_cast<float>(mousePos.x);
    float targetY = static_cast<float>(mousePos.y);

    const sf::FloatRect mockProjectileBounds{390.f, 290.f, 20.f, 20.f};
    
    // Rectángulo que simula al jugador (usamos el ratón como su posición) para recoger los núcleos
    sf::FloatRect playerBounds(targetX, targetY, 10.f, 10.f); 

    // 1. Actualizar Enemigos y verificar daño
    auto itEnemy = swarm.begin();
    while (itEnemy != swarm.end()) {
        itEnemy->update(targetX, targetY, deltaTime);
        
        if (itEnemy->isActive() && itEnemy->getBounds().intersects(mockProjectileBounds)) {
            itEnemy->takeDamage();
            
            // Si el daño redujo su HP a 0, soltar el núcleo
            if (!itEnemy->isActive()) {
                sf::Vector2f pos = itEnemy->getPosition();
                cores.append(Core(pos.x, pos.y));
            }
        }
        ++itEnemy;
    }

    // 2. Actualizar Núcleos y verificar recolección
    for (Core& core : cores) {
        if (core.isActive() && core.getBounds().intersects(playerBounds)) {
            core.collect();
            score += core.getValue(); // Sumar puntos
        }
    }

    // 3. LIMPIEZA SEGURA DE DESIGNAR
    // Borra de la memoria todo lo que esté inactivo sin romper los iteradores
    swarm.remove_if([](const Enemy& e) { return !e.isActive(); });
    cores.remove_if([](const Core& c) { return !c.isActive(); });
}

void Game::render() {
    window.clear(); 

    // Proyectil falso
    sf::RectangleShape mockProjectile(sf::Vector2f(20.f, 20.f));
    mockProjectile.setFillColor(sf::Color::Blue);
    mockProjectile.setPosition(390.f, 290.f);
    window.draw(mockProjectile);

    // Dibujar Núcleos
    for (Core& core : cores) {
        core.draw(window);
    }

    // Dibujar Enemigos
    auto itEnemy = swarm.begin();
    while (itEnemy != swarm.end()) {
        itEnemy->draw(window);
        ++itEnemy;
    }

    window.display(); 
}