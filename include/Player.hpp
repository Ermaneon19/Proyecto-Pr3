#pragma once
#include <SFML/Graphics.hpp>
#include <tpl_dynDlist.H>   
#include "Projectile.hpp"

using ProjectileList = Aleph::DynDlist<Projectile>;

class Player {
public:
    explicit Player(const sf::Vector2f& startPosition);

    void update(float dt, const sf::RenderWindow& window);

    void draw(sf::RenderTarget& target) const;

    sf::FloatRect getBounds() const;
    sf::Vector2f getPosition() const;
    void setPosition(const sf::Vector2f& pos);

    ProjectileList& getProjectiles() { return m_projectiles; }
    const ProjectileList& getProjectiles() const { return m_projectiles; }

private:
    void handleMovement(float dt, const sf::Vector2f& windowSize);
    void handleShooting(const sf::RenderWindow& window);
    void updateProjectiles(float dt, const sf::Vector2f& windowSize);

    sf::ConvexShape m_shape;
    float m_speed = 300.f;

    ProjectileList m_projectiles;   
    float m_fireCooldown = 0.25f;   
    float m_timeSinceShot = 0.25f;
};
