// Interfaz durante la partida: barra de XP, nombre y vida del jugador, escudo,
// impulso, habilidades, cronometro, puntaje, panel de subida de nivel y pausa.
#pragma once

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace cc {

class Game;

class Hud : public godot::CanvasLayer {
	GDCLASS(Hud, godot::CanvasLayer)

public:
	Game *game = nullptr;

	void _ready() override;
	void _process(double delta) override;
	void _unhandled_input(const godot::Ref<godot::InputEvent> &event) override;

	void toast(const godot::String &text);
	void show_levelup();
	void hide_levelup();
	void show_pause(bool visible);

protected:
	static void _bind_methods() {}

private:
	godot::Control *root = nullptr;
	godot::ProgressBar *xp_bar = nullptr;
	godot::Label *lvl_label = nullptr;
	godot::Label *name_label = nullptr;
	godot::ProgressBar *hp_bar = nullptr;
	godot::Label *hp_label = nullptr;
	godot::ProgressBar *shield_bar = nullptr;
	godot::ProgressBar *dash_bar = nullptr;
	godot::Label *dash_label = nullptr;
	godot::Label *abilities_label = nullptr;
	godot::Label *timer_label = nullptr;
	godot::Label *toast_label = nullptr;
	godot::Label *score_label = nullptr;
	godot::Label *kills_label = nullptr;
	godot::Label *cores_label = nullptr;
	godot::ProgressBar *float_hp = nullptr;
	godot::Control *levelup_layer = nullptr;
	godot::VBoxContainer *levelup_box = nullptr;
	godot::Control *pause_layer = nullptr;
	godot::Ref<godot::ShaderMaterial> vignette;

	float toast_time = 0.0f;
	float pulse_t = 0.0f;
	// Cache para no regenerar textos que no cambiaron.
	int last_score = -1, last_kills = -1, last_cores = -1, last_seconds = -1, last_level = -1, last_hp = -1, last_max_hp = -1;
	int last_abilities_hash = -1;
	float last_vignette = -1.0f;

	void build_top();
	void build_levelup();
	void build_pause();
	void refresh_abilities();

	void on_card_pressed(int index);
	void on_resume_pressed();
	void on_menu_pressed();
};

} // namespace cc
