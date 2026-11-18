#include "hud.h"

#include "game.h"
#include "game_data.h"
#include "mesh_util.h"
#include "player.h"
#include "ui_util.h"
#include "util.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace cc {

// Vineta roja que late cuando la vida esta baja.
static const char *SHADER_VIGNETTE = R"(
shader_type canvas_item;
uniform float intensity = 0.0;
void fragment() {
	float d = length(UV - 0.5);
	COLOR = vec4(0.9, 0.05, 0.05, smoothstep(0.3, 0.8, d) * intensity);
}
)";

static Control *full_rect(Control *c, bool ignore_mouse = true) {
	c->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	if (ignore_mouse) {
		c->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	}
	return c;
}

void Hud::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_process(false);
		return;
	}
	set_process_mode(Node::PROCESS_MODE_ALWAYS);

	root = memnew(Control);
	full_rect(root);
	root->set_theme(ui::make_theme());
	add_child(root);

	ColorRect *vg = memnew(ColorRect);
	full_rect(vg);
	vignette = mu::shader_material(SHADER_VIGNETTE);
	vg->set_material(vignette);
	root->add_child(vg);

	build_top();
	build_levelup();
	build_pause();
}

// ---------------------------------------------------------------- HUD en partida
void Hud::build_top() {
	// Barra de experiencia a todo lo ancho con el nivel encima.
	xp_bar = ui::bar(ui::BLUE, Color(0.75f, 0.6f, 0.3f), Vector2(0, 26));
	xp_bar->set_anchors_and_offsets_preset(Control::PRESET_TOP_WIDE);
	xp_bar->set_offset(SIDE_LEFT, 6);
	xp_bar->set_offset(SIDE_RIGHT, -6);
	xp_bar->set_offset(SIDE_TOP, 6);
	xp_bar->set_offset(SIDE_BOTTOM, 32);
	root->add_child(xp_bar);
	lvl_label = ui::label("NIVEL 1", 17, Color(1, 1, 1), true);
	full_rect(lvl_label);
	lvl_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	xp_bar->add_child(lvl_label);

	MarginContainer *margin = memnew(MarginContainer);
	full_rect(margin);
	margin->add_theme_constant_override("margin_left", 14);
	margin->add_theme_constant_override("margin_right", 14);
	margin->add_theme_constant_override("margin_top", 42);
	margin->add_theme_constant_override("margin_bottom", 14);
	root->add_child(margin);

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	margin->add_child(row);

	// --- Izquierda: jugador, vida, escudo, impulso, habilidades ---
	VBoxContainer *left = memnew(VBoxContainer);
	left->set_custom_minimum_size(Vector2(300, 0));
	left->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	left->add_theme_constant_override("separation", 4);
	row->add_child(left);

	name_label = ui::label(GameData::get().player_name.to_upper(), 20, ui::AMBER);
	name_label->set_clip_text(true);
	left->add_child(name_label);

	hp_bar = ui::bar(Color(0.82f, 0.12f, 0.12f), Color(0.3f, 0.05f, 0.05f), Vector2(300, 24));
	hp_bar->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	left->add_child(hp_bar);
	hp_label = ui::label("", 15, Color(1, 1, 1), true);
	full_rect(hp_label);
	hp_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	hp_bar->add_child(hp_label);

	shield_bar = ui::bar(ui::CYAN, Color(0.1f, 0.25f, 0.35f), Vector2(300, 9));
	shield_bar->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	shield_bar->set_visible(false);
	left->add_child(shield_bar);

	dash_bar = ui::bar(Color(0.95f, 0.9f, 0.5f), Color(0.3f, 0.28f, 0.1f), Vector2(150, 9));
	dash_bar->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	dash_bar->set_max(1.0);
	dash_bar->set_step(0.01);
	left->add_child(dash_bar);
	dash_label = ui::label("IMPULSO  [ESPACIO]", 13, ui::MUTED);
	left->add_child(dash_label);

	abilities_label = ui::label("", 15, Color(0.8f, 0.9f, 1.0f));
	left->add_child(abilities_label);

	// --- Centro: cronometro y avisos ---
	VBoxContainer *center = memnew(VBoxContainer);
	center->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	row->add_child(center);
	timer_label = ui::label("00:00", 42, Color(1, 1, 1), true);
	center->add_child(timer_label);
	toast_label = ui::label("", 26, ui::RED, true);
	toast_label->set_modulate(Color(1, 1, 1, 0));
	center->add_child(toast_label);

	// --- Derecha: puntaje, bajas, nucleos ---
	VBoxContainer *right = memnew(VBoxContainer);
	right->set_custom_minimum_size(Vector2(300, 0));
	right->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	right->add_theme_constant_override("separation", 0);
	row->add_child(right);
	Label *pts = ui::label("PUNTOS", 14, ui::MUTED);
	pts->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	right->add_child(pts);
	score_label = ui::label("0", 38, ui::AMBER);
	score_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	right->add_child(score_label);
	kills_label = ui::label("BAJAS  0", 19);
	kills_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	right->add_child(kills_label);
	cores_label = ui::label(U8("NÚCLEOS  0"), 19, Color(1.0f, 0.85f, 0.2f));
	cores_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	right->add_child(cores_label);

	// Barra de vida pequena que flota bajo el soldado.
	float_hp = ui::bar(Color(0.85f, 0.1f, 0.1f), Color(0, 0, 0), Vector2(56, 8));
	float_hp->set_size(Vector2(56, 8));
	root->add_child(float_hp);
}

void Hud::_process(double p_delta) {
	if (game == nullptr || game->player == nullptr) {
		return;
	}
	float delta = float(p_delta);
	Player *p = game->player;
	pulse_t += delta;

	xp_bar->set_max(game->xp_next);
	xp_bar->set_value(game->xp);
	if (game->level != last_level) {
		last_level = game->level;
		lvl_label->set_text(vformat("NIVEL %d", game->level));
	}

	int seconds = int(game->run_time);
	if (seconds != last_seconds) {
		last_seconds = seconds;
		timer_label->set_text(GameData::format_time(game->run_time));
	}
	int sc = game->score();
	if (sc != last_score) {
		last_score = sc;
		score_label->set_text(String::num_int64(sc));
	}
	if (game->kills != last_kills) {
		last_kills = game->kills;
		kills_label->set_text(vformat("BAJAS  %d", game->kills));
	}
	int cores = int(game->run_cores);
	if (cores != last_cores) {
		last_cores = cores;
		cores_label->set_text(U8("NÚCLEOS  ") + String::num_int64(cores));
	}

	// Vida
	hp_bar->set_max(p->max_hp);
	hp_bar->set_value(p->hp);
	int hp_i = int(std::ceil(p->hp)), max_i = int(p->max_hp);
	if (hp_i != last_hp || max_i != last_max_hp) {
		last_hp = hp_i;
		last_max_hp = max_i;
		hp_label->set_text(vformat("%d / %d", hp_i, max_i));
	}

	// Escudo
	bool has_shield = p->shield_max > 0.0f;
	if (shield_bar->is_visible() != has_shield) {
		shield_bar->set_visible(has_shield);
	}
	if (has_shield) {
		shield_bar->set_max(p->shield_max);
		shield_bar->set_value(p->shield);
	}

	// Impulso
	float ready = 1.0f - clampf(p->dash_cd / Player::DASH_COOLDOWN, 0.0f, 1.0f);
	dash_bar->set_value(ready);
	dash_label->set_modulate(ready >= 1.0f ? Color(1, 1, 1, 1) : Color(1, 1, 1, 0.45f));

	refresh_abilities();

	// Barra flotante bajo el jugador
	bool show_float = p->alive && game->camera != nullptr;
	float_hp->set_visible(show_float);
	if (show_float) {
		float_hp->set_max(p->max_hp);
		float_hp->set_value(p->hp);
		Vector2 screen = game->camera->unproject_position(p->get_global_position() + Vector3(0, -0.3f, 0.6f));
		float_hp->set_position(screen - Vector2(28, 0));
	}

	// Vineta de peligro
	float frac = p->max_hp > 0.0f ? p->hp / p->max_hp : 0.0f;
	float vg = 0.0f;
	if (!p->alive) {
		vg = 0.8f;
	} else if (frac < 0.35f) {
		vg = (0.35f - frac) / 0.35f * (0.45f + 0.2f * std::sin(pulse_t * 6.0f));
	}
	if (std::fabs(vg - last_vignette) > 0.005f) {
		last_vignette = vg;
		vignette->set_shader_parameter("intensity", vg);
	}

	// Aviso temporal
	if (toast_time > 0.0f) {
		toast_time -= delta;
		float a = toast_time > 2.5f ? (2.7f - toast_time) / 0.2f : std::min(1.0f, toast_time / 0.5f);
		toast_label->set_modulate(Color(1, 1, 1, clampf(a, 0.0f, 1.0f)));
	}
}

void Hud::refresh_abilities() {
	const int *t = game->taken;
	int hash = game->rifle_level();
	static const int shown[] = { U_STRIKE, U_DRONES, U_NOVA, U_CHAIN, U_MISSILE, U_SHIELD };
	for (int id : shown) {
		hash = hash * 31 + t[id];
	}
	if (hash == last_abilities_hash) {
		return;
	}
	last_abilities_hash = hash;
	String s = vformat("Rifle  Nv.%d", game->rifle_level());
	for (int id : shown) {
		if (t[id] > 0) {
			s += String("\n") + String::utf8(Game::UPGRADES[id].title) + vformat("  Nv.%d", t[id]);
		}
	}
	abilities_label->set_text(s);
}

void Hud::toast(const String &text) {
	toast_label->set_text(text);
	toast_time = 2.7f;
}

// ---------------------------------------------------------------- subida de nivel
void Hud::build_levelup() {
	levelup_layer = memnew(Control);
	full_rect(levelup_layer, false);
	levelup_layer->set_visible(false);
	root->add_child(levelup_layer);

	ColorRect *dim = memnew(ColorRect);
	dim->set_color(Color(0, 0, 0, 0.6f));
	full_rect(dim, false);
	levelup_layer->add_child(dim);

	CenterContainer *center = memnew(CenterContainer);
	full_rect(center, false);
	levelup_layer->add_child(center);
	PanelContainer *panel = memnew(PanelContainer);
	center->add_child(panel);
	levelup_box = memnew(VBoxContainer);
	levelup_box->add_theme_constant_override("separation", 12);
	panel->add_child(levelup_box);
}

void Hud::show_levelup() {
	for (int i = levelup_box->get_child_count() - 1; i >= 0; i--) {
		Node *c = levelup_box->get_child(i);
		levelup_box->remove_child(c);
		c->queue_free();
	}
	levelup_box->add_child(ui::label(U8("¡SUBISTE DE NIVEL!"), 34, ui::AMBER, true));
	levelup_box->add_child(ui::label(vformat("Nivel %d  -  elige una mejora (teclas 1, 2, 3)", game->level), 15, ui::MUTED, true));

	Button *first = nullptr;
	for (int i = 0; i < int(game->levelup_options.size()); i++) {
		int id = game->levelup_options[i];
		const UpgradeDef &def = Game::UPGRADES[id];
		int lvl = game->taken[id];
		bool is_new = def.ability && lvl == 0;

		Button *btn = memnew(Button);
		btn->set_custom_minimum_size(Vector2(560, 86));
		MarginContainer *m = memnew(MarginContainer);
		full_rect(m);
		m->add_theme_constant_override("margin_left", 14);
		m->add_theme_constant_override("margin_right", 14);
		m->add_theme_constant_override("margin_top", 10);
		m->add_theme_constant_override("margin_bottom", 10);
		btn->add_child(m);
		HBoxContainer *h = memnew(HBoxContainer);
		h->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		h->add_theme_constant_override("separation", 14);
		m->add_child(h);

		Label *key = ui::label(String::num_int64(i + 1), 30, ui::AMBER, true);
		key->set_custom_minimum_size(Vector2(34, 0));
		key->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		h->add_child(key);

		VBoxContainer *v = memnew(VBoxContainer);
		v->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		v->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		v->add_theme_constant_override("separation", 2);
		h->add_child(v);
		v->add_child(ui::label(String::utf8(def.title), 22, def.ability ? ui::CYAN : Color(1, 1, 1)));
		v->add_child(ui::label(String::utf8(def.desc), 15, Color(0.75f, 0.8f, 0.85f)));

		Label *tag = ui::label(is_new ? String("NUEVO") : vformat("Nv. %d", lvl + 1), 18, is_new ? Color(0.4f, 1.0f, 0.5f) : ui::AMBER);
		tag->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		h->add_child(tag);

		btn->connect("pressed", callable_mp(this, &Hud::on_card_pressed).bind(i));
		levelup_box->add_child(btn);
		if (first == nullptr) {
			first = btn;
		}
	}
	// Sin foco inicial a proposito: ESPACIO es el impulso y no debe elegir una carta por accidente.
	(void)first;
	levelup_layer->set_visible(true);
}

void Hud::hide_levelup() {
	levelup_layer->set_visible(false);
}

void Hud::on_card_pressed(int index) {
	game->choose_upgrade(index);
}

// ---------------------------------------------------------------- pausa
void Hud::build_pause() {
	pause_layer = memnew(Control);
	full_rect(pause_layer, false);
	pause_layer->set_visible(false);
	root->add_child(pause_layer);

	ColorRect *dim = memnew(ColorRect);
	dim->set_color(Color(0, 0, 0, 0.65f));
	full_rect(dim, false);
	pause_layer->add_child(dim);

	CenterContainer *center = memnew(CenterContainer);
	full_rect(center, false);
	pause_layer->add_child(center);
	PanelContainer *panel = memnew(PanelContainer);
	panel->set_custom_minimum_size(Vector2(420, 0));
	center->add_child(panel);
	VBoxContainer *v = memnew(VBoxContainer);
	v->add_theme_constant_override("separation", 12);
	panel->add_child(v);

	v->add_child(ui::label("PAUSA", 40, ui::AMBER, true));
	v->add_child(ui::label(U8("WASD: moverse  ·  ESPACIO: impulso  ·  ESC: pausa"), 14, ui::MUTED, true));
	Button *resume = ui::button("Continuar");
	resume->set_name("Resume");
	resume->connect("pressed", callable_mp(this, &Hud::on_resume_pressed));
	v->add_child(resume);
	Button *menu = ui::button(U8("Menú Principal"));
	menu->connect("pressed", callable_mp(this, &Hud::on_menu_pressed));
	v->add_child(menu);
}

void Hud::show_pause(bool visible) {
	pause_layer->set_visible(visible);
	if (visible) {
		Button *resume = Object::cast_to<Button>(pause_layer->find_child("Resume", true, false));
		if (resume != nullptr) {
			resume->grab_focus();
		}
	}
}

void Hud::on_resume_pressed() {
	if (game->state == Game::PAUSED) {
		game->toggle_pause();
	}
}

void Hud::on_menu_pressed() {
	game->quit_to_menu();
}

// ---------------------------------------------------------------- teclado
void Hud::_unhandled_input(const Ref<InputEvent> &event) {
	if (game == nullptr) {
		return;
	}
	if (event->is_action_pressed("pause")) {
		if (game->state == Game::PLAYING || game->state == Game::PAUSED) {
			game->toggle_pause();
			get_viewport()->set_input_as_handled();
		}
		return;
	}
	if (game->state == Game::LEVELUP) {
		Ref<InputEventKey> key = event;
		if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
			int index = -1;
			switch (key->get_physical_keycode()) {
				case KEY_1:
				case KEY_KP_1:
					index = 0;
					break;
				case KEY_2:
				case KEY_KP_2:
					index = 1;
					break;
				case KEY_3:
				case KEY_KP_3:
					index = 2;
					break;
				default:
					break;
			}
			if (index >= 0 && index < int(game->levelup_options.size())) {
				get_viewport()->set_input_as_handled();
				game->choose_upgrade(index);
			}
		}
	}
}

} // namespace cc
