#include "Game.hpp"

#include <imgui-SFML.h>
#include <imgui.h>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/System/Clock.hpp>
#include <stdexcept>

Game::Game()
    : window_(sf::VideoMode(800, 600), "Proyecto-Pr3") {
    window_.setVerticalSyncEnabled(true);

    imguiInitialized_ = ImGui::SFML::Init(window_);
    if (!imguiInitialized_) {
        throw std::runtime_error("No se pudo inicializar ImGui-SFML.");
    }
}

Game::~Game() {
    if (imguiInitialized_) {
        ImGui::SFML::Shutdown(window_);
    }
}

void Game::run() {
    sf::Clock deltaClock;

    while (window_.isOpen()) {
        processEvents();
        update(deltaClock.restart());
        render();
    }
}

void Game::processEvents() {
    sf::Event event{};
    while (window_.pollEvent(event)) {
        ImGui::SFML::ProcessEvent(window_, event);
        if (event.type == sf::Event::Closed) {
            window_.close();
        }
    }
}

void Game::update(sf::Time deltaTime) {
    ImGui::SFML::Update(window_, deltaTime);
    ImGui::ShowDemoWindow();
}

void Game::render() {
    window_.clear(sf::Color(30, 30, 30));
    ImGui::SFML::Render(window_);
    window_.display();
}