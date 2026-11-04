#pragma once
#include <SFML/Graphics.hpp>

class Enemy {
private:
    sf::RectangleShape shape;
    float speed;
    int hp;
    bool active; // NUEVO: Para saber si el enemigo sigue vivo

public:
    Enemy(float startX, float startY);
    
    void update(float targetX, float targetY, float deltaTime);
    void draw(sf::RenderWindow& window);
    sf::FloatRect getBounds() const;
    
    // NUEVO: Métodos para recibir daño y consultar su estado
    void takeDamage();
    bool isActive() const;

    sf::Vector2f getPosition() const;
};