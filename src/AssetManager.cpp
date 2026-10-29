#include "AssetManager.hpp"

#include <stdexcept>

const sf::Texture& AssetManager::getTexture(const std::string& path) {
    const auto [texture, inserted] = textures_.try_emplace(path);
    if (inserted && !texture->second.loadFromFile(path)) {
        textures_.erase(texture);
        throw std::runtime_error("No se pudo cargar la textura: " + path);
    }

    return texture->second;
}
