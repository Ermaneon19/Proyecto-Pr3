// Piezas de interfaz compartidas por el HUD y todas las pantallas:
// tema, cajas de estilo, etiquetas, botones y fondo animado.
#pragma once

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace cc {
namespace ui {

extern const godot::Color AMBER;
extern const godot::Color BLUE;
extern const godot::Color CYAN;
extern const godot::Color RED;
extern const godot::Color MUTED;
extern const godot::Color PANEL_BG;

godot::Ref<godot::Theme> make_theme();
godot::Ref<godot::StyleBoxFlat> box(const godot::Color &bg, const godot::Color &border, int border_width = 0, int pad = 10, int radius = 4);

godot::Label *label(const godot::String &text, int size = 18, const godot::Color &color = godot::Color(1, 1, 1), bool centered = false);
godot::Button *button(const godot::String &text, int font_size = 22, const godot::Vector2 &min_size = godot::Vector2(0, 52));
godot::ProgressBar *bar(const godot::Color &fill, const godot::Color &border, const godot::Vector2 &min_size);

// Prepara la raiz de una pantalla completa (tema + fondo animado) y devuelve
// un VBox centrado dentro de un panel donde colocar el contenido.
godot::VBoxContainer *screen_panel(godot::Control *root, float min_width, int separation = 12);

} // namespace ui
} // namespace cc
