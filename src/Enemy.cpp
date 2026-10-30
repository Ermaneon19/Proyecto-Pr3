#include "Enemy.hpp"
#include <cmath>

Enemy::Enemy(float startX, float startY) : speed(100.0f), hp(3) {
    shape.setSize(sf::Vector2f(32.0f, 32.0f)); 
    shape.setFillColor(sf::Color::Red);
    shape.setPosition(startX, startY);
}

void Enemy::update(float targetX, float targetY, float deltaTime) {
    sf::Vector2f currentPos = shape.getPosition();

    float dx = targetX - currentPos.x;
    float dy = targetY - currentPos.y;

    float distance = std::sqrt(dx * dx + dy * dy);

    if (distance > 0.0f) {
        dx /= distance;
        dy /= distance;
    }

    shape.move(dx * speed * deltaTime, dy * speed * deltaTime);
}

void Enemy::draw(sf::RenderWindow& window) {
    window.draw(shape);
}

sf::FloatRect Enemy::getBounds() const {
    return shape.getGlobalBounds();
}