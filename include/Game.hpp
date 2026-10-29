#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Time.hpp>

class Game {
public:
    enum class State {
        MainMenu,
        Playing,
        GameOver
    };

    Game();
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void run();
    void finishGame(int finalScore);

private:
    void processEvents();
    void update(sf::Time deltaTime);
    void render();

    sf::RenderWindow window_;
    bool imguiInitialized_{false};
    State state_{State::MainMenu};
    int finalScore_{0};
};