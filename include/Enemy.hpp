#pragma once
#include <SFML/Graphics.hpp>

class Enemy {
private:
    sf::Sprite sprite;
    float speed;
    int hp;
    bool active;

    // Control de animación
    int currentFrame;
    float animationTimer;
    int frameWidth;
    int frameHeight;

public:
    // El constructor ahora exige recibir la textura ya cargada
    Enemy(float startX, float startY, const sf::Texture& texture);
    
    void update(float targetX, float targetY, float deltaTime);
    void draw(sf::RenderWindow& window);
    sf::FloatRect getBounds() const;
    
    void takeDamage();
    bool isActive() const;
    sf::Vector2f getPosition() const;
};