#pragma once

#include <SFML/Graphics/Texture.hpp>

#include <string>
#include <unordered_map>

class AssetManager {
public:
    const sf::Texture& getTexture(const std::string& path);

private:
    std::unordered_map<std::string, sf::Texture> textures_;
};