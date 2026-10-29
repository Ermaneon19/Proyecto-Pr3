#include "Game.hpp"

#include <imgui-SFML.h>
#include <imgui.h>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <stdexcept>

Game::Game()
    : window_(sf::VideoMode(800, 600), "Ciber Arena") {
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
        } else if (event.type == sf::Event::KeyPressed &&
                   event.key.code == sf::Keyboard::Escape) {
            if (state_ == State::Playing) {
                state_ = State::MainMenu;
            } else {
                window_.close();
            }
        }
    }
}

void Game::update(sf::Time deltaTime) {
    ImGui::SFML::Update(window_, deltaTime);

    ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Always);
    ImGui::Begin("Ciber Arena");
    ImGui::TextUnformatted("Supervivencia incremental en la arena");
    ImGui::Separator();

    switch (state_) {
    case State::MainMenu:
        ImGui::TextWrapped("Sobrevive a las oleadas, destruye enemigos y recoge ciber-nucleos.");
        if (ImGui::Button("Iniciar partida")) {
            finalScore_ = 0;
            state_ = State::Playing;
        }
        ImGui::TextUnformatted("WASD o flechas: moverse | Espacio o clic: disparar");
        ImGui::TextUnformatted("Esc: volver al menu");
        break;
    case State::Playing:
        ImGui::TextUnformatted("Partida en curso");
        ImGui::TextUnformatted("Jugador y enemigos: modulos en integracion.");
        ImGui::TextUnformatted("Esc: volver al menu");
        break;
    case State::GameOver:
        ImGui::TextUnformatted("Fin de la partida");
        ImGui::Text("Puntaje final: %d", finalScore_);
        if (ImGui::Button("Jugar de nuevo")) {
            finalScore_ = 0;
            state_ = State::Playing;
        }
        ImGui::SameLine();
        if (ImGui::Button("Menu principal")) {
            state_ = State::MainMenu;
        }
        break;
    }

    ImGui::End();
}

void Game::render() {
    window_.clear(sf::Color(12, 16, 29));
    ImGui::SFML::Render(window_);
    window_.display();
}

void Game::finishGame(int finalScore) {
    if (state_ == State::Playing) {
        finalScore_ = finalScore;
        state_ = State::GameOver;
    }
}