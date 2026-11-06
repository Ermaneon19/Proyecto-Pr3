#include "Player.hpp"

Player::Player(const sf::Vector2f& startPosition)
{
    m_shape.setPointCount(3);
    m_shape.setPoint(0, sf::Vector2f(0.f, -20.f));
    m_shape.setPoint(1, sf::Vector2f(16.f, 16.f));
    m_shape.setPoint(2, sf::Vector2f(-16.f, 16.f));
    m_shape.setFillColor(sf::Color(80, 200, 255));
    m_shape.setOutlineThickness(2.f);
    m_shape.setOutlineColor(sf::Color::White);
    m_shape.setPosition(startPosition);
}

void Player::update(float dt, const sf::RenderWindow& window)
{
    sf::Vector2f winSize(static_cast<float>(window.getSize().x),
                         static_cast<float>(window.getSize().y));

    m_timeSinceShot += dt;

    handleMovement(dt, winSize);
    handleShooting(window);
    updateProjectiles(dt, winSize);
}

void Player::handleMovement(float dt, const sf::Vector2f& windowSize)
{
    sf::Vector2f dir(0.f, 0.f);

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))    dir.y -= 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))  dir.y += 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  dir.x -= 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) dir.x += 1.f;

    if (dir.x != 0.f && dir.y != 0.f) {
        dir.x *= 0.70710678f;
        dir.y *= 0.70710678f;
    }

    m_shape.move(dir * m_speed * dt);

    sf::FloatRect b = m_shape.getGlobalBounds();
    sf::Vector2f pos = m_shape.getPosition();
    if (b.left < 0.f)                      pos.x += -b.left;
    if (b.top < 0.f)                       pos.y += -b.top;
    if (b.left + b.width > windowSize.x)   pos.x -= (b.left + b.width - windowSize.x);
    if (b.top + b.height > windowSize.y)   pos.y -= (b.top + b.height - windowSize.y);
    m_shape.setPosition(pos);
}

void Player::handleShooting(const sf::RenderWindow& window)
{
    if (m_timeSinceShot < m_fireCooldown)
        return;

    bool click = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    bool space = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    if (!click && !space)
        return;

    sf::Vector2f origin = m_shape.getPosition();
    sf::Vector2f mouse  = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    sf::Vector2f direction = mouse - origin;

    m_projectiles.append(Projectile(origin, direction));
    m_timeSinceShot = 0.f;
}

void Player::updateProjectiles(float dt, const sf::Vector2f& windowSize)
{

    for (ProjectileList::Iterator it(m_projectiles); it.has_curr(); )
    {
        Projectile& p = it.get_curr();
        p.update(dt);

        if (!p.isAlive() || p.isOffscreen(windowSize))
            it.del();      
        else
            it.next();
    }
}

void Player::draw(sf::RenderTarget& target) const
{
    for (ProjectileList::Iterator it(const_cast<ProjectileList&>(m_projectiles));
         it.has_curr(); it.next())
        it.get_curr().draw(target);

    target.draw(m_shape);
}

sf::FloatRect Player::getBounds() const { return m_shape.getGlobalBounds(); }
sf::Vector2f Player::getPosition() const { return m_shape.getPosition(); }
void Player::setPosition(const sf::Vector2f& pos) { m_shape.setPosition(pos); }
