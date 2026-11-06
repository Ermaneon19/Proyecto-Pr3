#include "Enemy.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Enemy::Enemy(float startX, float startY, const sf::Texture& texture) 
    : speed(100.0f), hp(3), active(true), currentFrame(0), animationTimer(0.0f) {
    
    sprite.setTexture(texture);
    
    // Calcula el tamaño del frame dinámicamente (ancho total / 6 frames)
    frameWidth = texture.getSize().x / 6;
    frameHeight = texture.getSize().y;

    // Recorta el primer frame
    sprite.setTextureRect(sf::IntRect(0, 0, frameWidth, frameHeight));
    sprite.setScale(0.1f, 0.1f);
    
    // Centra el punto de origen para que la rotación sea perfecta
    sprite.setOrigin(frameWidth / 2.0f, frameHeight / 2.0f);
    sprite.setPosition(startX, startY);
}

void Enemy::update(float targetX, float targetY, float deltaTime) {
    sf::Vector2f currentPos = sprite.getPosition();

    float dx = targetX - currentPos.x;
    float dy = targetY - currentPos.y;
    float distance = std::sqrt(dx * dx + dy * dy);

    if (distance > 0.0f) {
        dx /= distance;
        dy /= distance;
        
        // Calcular el ángulo de rotación en grados hacia el objetivo
        float angle = std::atan2(dy, dx) * 180.0f / M_PI;
        sprite.setRotation(angle);
    }

    sprite.move(dx * speed * deltaTime, dy * speed * deltaTime);

    // Sistema de animación: cambia de fotograma cada 0.1 segundos
    animationTimer += deltaTime;
    if (animationTimer >= 0.1f) {
        animationTimer = 0.0f;
        currentFrame = (currentFrame + 1) % 6; // Cicla entre 0 y 5
        sprite.setTextureRect(sf::IntRect(currentFrame * frameWidth, 0, frameWidth, frameHeight));
    }
}

void Enemy::draw(sf::RenderWindow& window) {
    window.draw(sprite);
}

sf::FloatRect Enemy::getBounds() const {
    return sprite.getGlobalBounds();
}

void Enemy::takeDamage() {
    hp--;
    if (hp <= 0) {
        active = false;
        sprite.setColor(sf::Color(100, 100, 100)); 
    }
}

bool Enemy::isActive() const {
    return active;
}

sf::Vector2f Enemy::getPosition() const {
    return sprite.getPosition();
}