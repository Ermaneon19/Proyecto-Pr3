#include "Projectile.hpp"
#include <cmath>

Projectile::Projectile(const sf::Vector2f& position, const sf::Vector2f& direction,
                       float speed, float radius)
{
    m_shape.setRadius(radius);
    m_shape.setOrigin(radius, radius);
    m_shape.setPosition(position);
    m_shape.setFillColor(sf::Color(255, 220, 60));
    m_shape.setOutlineThickness(1.f);
    m_shape.setOutlineColor(sf::Color(255, 140, 0));

    float len = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    sf::Vector2f dir = (len > 0.f) ? sf::Vector2f(direction.x / len, direction.y / len)
                                   : sf::Vector2f(0.f, -1.f);
    m_velocity = dir * speed;
}

void Projectile::update(float dt)
{
    m_shape.move(m_velocity * dt);
}

void Projectile::draw(sf::RenderTarget& target) const
{
    target.draw(m_shape);
}

bool Projectile::isOffscreen(const sf::Vector2f& bounds) const
{
    sf::Vector2f p = m_shape.getPosition();
    float r = m_shape.getRadius();
    return p.x < -r || p.y < -r || p.x > bounds.x + r || p.y > bounds.y + r;
}

sf::FloatRect Projectile::getBounds() const
{
    return m_shape.getGlobalBounds();
}

sf::Vector2f Projectile::getPosition() const
{
    return m_shape.getPosition();
}
