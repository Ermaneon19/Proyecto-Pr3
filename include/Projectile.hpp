#pragma once
#include <SFML/Graphics.hpp>

class Projectile {
public:
    Projectile(const sf::Vector2f& position, const sf::Vector2f& direction,
               float speed = 600.f, float radius = 5.f);

    void update(float dt);
    void draw(sf::RenderTarget& target) const;

    bool isOffscreen(const sf::Vector2f& bounds) const;

    sf::FloatRect getBounds() const;
    sf::Vector2f getPosition() const;

    void destroy() { m_alive = false; }
    bool isAlive() const { return m_alive; }

private:
    sf::CircleShape m_shape;
    sf::Vector2f m_velocity;
    bool m_alive = true;
};
