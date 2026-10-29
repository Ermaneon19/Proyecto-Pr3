#include "Enemy.hpp"

#include <cmath>

Enemy::Enemy(float startX, float startY)
    : shape(sf::Vector2f(32.0f, 32.0f)), speed(120.0f), hp(3) {
    shape.setPosition(startX, startY);
    shape.setFillColor(sf::Color::Red);
}

void Enemy::update(float targetX, float targetY, float deltaTime) {
    const auto currentPosition = shape.getPosition();
    const float targetCenterX = targetX + 16.0f;
    const float targetCenterY = targetY + 16.0f;
    const float enemyCenterX = currentPosition.x + shape.getSize().x / 2.0f;
    const float enemyCenterY = currentPosition.y + shape.getSize().y / 2.0f;

    const float deltaX = targetCenterX - enemyCenterX;
    const float deltaY = targetCenterY - enemyCenterY;
    const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);

    if (distance > 0.0f) {
        const float stepX = (deltaX / distance) * speed * deltaTime;
        const float stepY = (deltaY / distance) * speed * deltaTime;
        shape.move(stepX, stepY);
    }
}

void Enemy::draw(sf::RenderWindow& window) {
    window.draw(shape);
}

sf::FloatRect Enemy::getBounds() const {
    return shape.getGlobalBounds();
}
