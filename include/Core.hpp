#pragma once
#include <SFML/Graphics.hpp>

class Core {
private:
    sf::CircleShape shape;
    int pointValue;
    bool active;

public:
    Core(float startX, float startY);
    void draw(sf::RenderWindow& window);
    sf::FloatRect getBounds() const;
    int getValue() const;
    void collect();
    bool isActive() const;
};