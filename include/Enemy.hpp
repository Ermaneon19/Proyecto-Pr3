#pragma once
#include <SFML/Graphics.hpp>

class Enemy {
private:
    sf::RectangleShape shape;
    float speed;
    int hp;

public:
    Enemy(float startX, float startY);
    
    void update(float targetX, float targetY, float deltaTime);
    void draw(sf::RenderWindow& window);
    sf::FloatRect getBounds() const;
};