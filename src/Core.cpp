#include "Core.hpp"

Core::Core(float startX, float startY) : pointValue(10), active(true) {
    shape.setRadius(8.0f);
    shape.setFillColor(sf::Color::Yellow); // Amarillo para el botín
    shape.setPosition(startX, startY);
}

void Core::draw(sf::RenderWindow& window) {
    window.draw(shape);
}

sf::FloatRect Core::getBounds() const {
    return shape.getGlobalBounds();
}

int Core::getValue() const {
    return pointValue;
}

void Core::collect() {
    active = false;
}

bool Core::isActive() const {
    return active;
}