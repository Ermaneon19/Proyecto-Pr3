#include "screens.h"

#include "game_data.h"
#include "ui_util.h"
#include "util.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_separator.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>

using namespace godot;

namespace cc {

const char *SCENE_MENU = "res://scenes/main_menu.tscn";
const char *SCENE_REGISTER = "res://scenes/register.tscn";
const char *SCENE_GAME = "res://scenes/game.tscn";
const char *SCENE_GAME_OVER = "res://scenes/game_over.tscn";
const char *SCENE_RANKING = "res://scenes/ranking.tscn";
const char *SCENE_SHOP = "res://scenes/shop.tscn";

// ---------------------------------------------------------------- base
bool ScreenBase::in_editor() const {
	return Engine::get_singleton()->is_editor_hint();
}

void ScreenBase::go_to(const char *scene) {
	SceneTree *tree = get_tree();
	if (tree != nullptr) {
		tree->set_pause(false);
		tree->change_scene_to_file(scene);
	}
}

void ScreenBase::go_menu() {
	go_to(SCENE_MENU);
}
void ScreenBase::go_register() {
	go_to(SCENE_REGISTER);
}
void ScreenBase::go_game() {
	go_to(SCENE_GAME);
}
void ScreenBase::go_ranking() {
	go_to(SCENE_RANKING);
}
void ScreenBase::go_shop() {
	go_to(SCENE_SHOP);
}

void ScreenBase::_unhandled_input(const Ref<InputEvent> &event) {
	if (back_scene != nullptr && !in_editor() && event->is_action_pressed("ui_cancel")) {
		get_viewport()->set_input_as_handled();
		go_to(back_scene);
	}
}

// ---------------------------------------------------------------- pantalla de inicio
void MainMenu::_ready() {
	if (in_editor()) {
		return;
	}
	GameData::setup_input();
	GameData &gd = GameData::get();
	VBoxContainer *v = ui::screen_panel(this, 520, 12);

	v->add_child(ui::label(U8("CÓDIGO CRÍTICO"), 54, ui::AMBER, true));
	v->add_child(ui::label(U8("Arena survivor incremental  ·  3D"), 17, ui::MUTED, true));
	v->add_child(memnew(HSeparator));

	Button *start = ui::button("Iniciar Juego", 26, Vector2(0, 62));
	start->connect("pressed", callable_mp(static_cast<ScreenBase *>(this), &ScreenBase::go_register));
	v->add_child(start);

	Button *ranking = ui::button("Ver Ranking");
	ranking->connect("pressed", callable_mp(static_cast<ScreenBase *>(this), &ScreenBase::go_ranking));
	v->add_child(ranking);

	Button *shop = ui::button(U8("Armería  (") + String::num_int64(gd.cores) + U8(" núcleos)"));
	shop->connect("pressed", callable_mp(static_cast<ScreenBase *>(this), &ScreenBase::go_shop));
	v->add_child(shop);

	Button *quit = ui::button("Salir");
	quit->connect("pressed", callable_mp(this, &MainMenu::on_quit));
	v->add_child(quit);

	v->add_child(memnew(HSeparator));
	String record = gd.best_score > 0
			? U8("Mejor puntaje: ") + String::num_int64(gd.best_score) + U8("  ·  Récord: ") + GameData::format_time(gd.best_time) + U8("  ·  ") + String::num_int64(gd.best_kills) + " bajas"
			: U8("Sobrevive a la plaga de insectos mecánicos");
	v->add_child(ui::label(record, 14, ui::MUTED, true));
	v->add_child(ui::label(U8("WASD: moverse  ·  ESPACIO: impulso  ·  ESC: pausa  ·  disparo automático"), 13, Color(0.5f, 0.54f, 0.58f), true));

	start->grab_focus();
}

void MainMenu::on_quit() {
	get_tree()->quit();
}

// ---------------------------------------------------------------- pantalla de registro
void RegisterScreen::_ready() {
	if (in_editor()) {
		return;
	}
	GameData::setup_input();
	back_scene = SCENE_MENU;
	VBoxContainer *v = ui::screen_panel(this, 560, 12);

	v->add_child(ui::label("REGISTRO", 42, ui::AMBER, true));
	v->add_child(ui::label(U8("Escribe tu nombre, equipo o alias.\nTus puntos quedarán asociados a él en el ranking."), 16, ui::MUTED, true));

	name_edit = memnew(LineEdit);
	name_edit->set_placeholder("Nombre, equipo o alias");
	name_edit->set_max_length(16);
	name_edit->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	name_edit->set_custom_minimum_size(Vector2(0, 58));
	name_edit->add_theme_font_size_override("font_size", 26);
	name_edit->set_text(GameData::get().player_name);
	name_edit->connect("text_submitted", callable_mp(this, &RegisterScreen::on_submitted));
	v->add_child(name_edit);

	error_label = ui::label(" ", 15, ui::RED, true);
	v->add_child(error_label);

	Button *start = ui::button("Comenzar", 26, Vector2(0, 60));
	start->connect("pressed", callable_mp(this, &RegisterScreen::on_start));
	v->add_child(start);

	Button *back = ui::button("Volver");
	back->connect("pressed", callable_mp(static_cast<ScreenBase *>(this), &ScreenBase::go_menu));
	v->add_child(back);

	name_edit->grab_focus();
	name_edit->set_caret_column(name_edit->get_text().length());
}

void RegisterScreen::on_submitted(const String &text) {
	on_start();
}

void RegisterScreen::on_start() {
	String name = name_edit->get_text().strip_edges();
	if (name.length() < 2) {
		error_label->set_text(U8("Escribe al menos 2 caracteres para continuar."));
		name_edit->grab_focus();
		return;
	}
	GameData &gd = GameData::get();
	gd.player_name = name;
	gd.save_game(); // recuerda el ultimo nombre usado
	go_game();
}

// ---------------------------------------------------------------- pantalla de game over
void GameOverScreen::_ready() {
	if (in_editor()) {
		return;
	}
	GameData::setup_input();
	const GameData &gd = GameData::get();
	const RunResult &r = gd.last_run;
	VBoxContainer *v = ui::screen_panel(this, 600, 10);

	v->add_child(ui::label("FIN DEL JUEGO", 52, ui::RED, true));
	String who = r.valid ? r.name : gd.player_name;
	v->add_child(ui::label(who.to_upper() + U8(", tu soldado ha caído"), 18, ui::MUTED, true));
	v->add_child(memnew(HSeparator));

	v->add_child(ui::label("PUNTAJE TOTAL", 16, ui::MUTED, true));
	v->add_child(ui::label(String::num_int64(r.score), 76, ui::AMBER, true));

	if (r.valid) {
		String stats = U8("Tiempo ") + GameData::format_time(r.time) + U8("   ·   ") + String::num_int64(r.kills) + U8(" bajas   ·   Nivel ") + String::num_int64(r.level);
		v->add_child(ui::label(stats, 18, Color(1, 1, 1), true));
		String rank = U8("Posición en el ranking: #") + String::num_int64(r.rank);
		if (r.rank == 1) {
			rank += U8("   ¡NUEVO RÉCORD!");
		}
		v->add_child(ui::label(rank, 20, r.rank <= 3 ? Color(0.4f, 1.0f, 0.5f) : ui::CYAN, true));
		v->add_child(ui::label(U8("+") + String::num_int64(r.earned) + U8(" núcleos para la Armería"), 16, Color(1.0f, 0.85f, 0.2f), true));
	}
	v->add_child(memnew(HSeparator));

	Button *retry = ui::button("Reintentar", 26, Vector2(0, 60));
	retry->connect("pressed", callable_mp(static_cast<ScreenBase *>(this), &ScreenBase::go_game));
	v->add_child(retry);

	Button *ranking = ui::button("Ver Ranking");
	ranking->connect("pressed", callable_mp(static_cast<ScreenBase *>(this), &ScreenBase::go_ranking));
	v->add_child(ranking);

	Button *menu = ui::button(U8("Menú Principal"));
	menu->connect("pressed", callable_mp(static_cast<ScreenBase *>(this), &ScreenBase::go_menu));
	v->add_child(menu);

	retry->grab_focus();
}

// ---------------------------------------------------------------- pantalla de ranking
void RankingScreen::_ready() {
	if (in_editor()) {
		return;
	}
	GameData::setup_input();
	back_scene = SCENE_MENU;
	const GameData &gd = GameData::get();
	VBoxContainer *v = ui::screen_panel(this, 760, 10);

	v->add_child(ui::label("RANKING", 44, ui::AMBER, true));
	v->add_child(ui::label("Los 10 mejores puntajes", 15, ui::MUTED, true));
	if (const PlayerStats *stats = gd.find_player_stats(gd.player_name)) {
		v->add_child(ui::label(
				U8("Tus partidas: ") + String::num_int64(stats->games_played) +
						U8("   ·   Récord personal: ") + String::num_int64(stats->best_score) +
						U8("   ·   Bajas acumuladas: ") + String::num_int64(stats->total_kills),
				16, ui::CYAN, true));
	}
	v->add_child(memnew(HSeparator));

	if (gd.ranking.empty()) {
		v->add_child(ui::label(U8("Todavía no hay partidas registradas.\n¡Sé el primero en aparecer aquí!"), 18, ui::MUTED, true));
	} else {
		GridContainer *grid = memnew(GridContainer);
		grid->set_columns(6);
		grid->add_theme_constant_override("h_separation", 26);
		grid->add_theme_constant_override("v_separation", 6);
		v->add_child(grid);

		static const char *headers[6] = { "#", "JUGADOR", "PUNTOS", "BAJAS", "NIVEL", "TIEMPO" };
		for (int c = 0; c < 6; c++) {
			Label *h = ui::label(headers[c], 14, ui::MUTED);
			if (c == 1) {
				h->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			} else if (c >= 2) {
				h->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
			}
			grid->add_child(h);
		}

		// Resalta la partida recien jugada (si entro en el top 10).
		int highlight = gd.last_run.valid ? gd.last_run.rank - 1 : -1;
		int rows = std::min(10, int(gd.ranking.size()));
		for (int i = 0; i < rows; i++) {
			const RankEntry &e = gd.ranking[i];
			Color col(1, 1, 1);
			if (i == highlight) {
				col = Color(0.4f, 1.0f, 0.5f);
			} else if (i == 0) {
				col = Color(1.0f, 0.85f, 0.2f);
			} else if (i == 1) {
				col = Color(0.82f, 0.86f, 0.9f);
			} else if (i == 2) {
				col = Color(0.9f, 0.6f, 0.35f);
			}
			int size = i < 3 ? 22 : 19;
			String cells[6] = {
				String::num_int64(i + 1),
				e.name,
				String::num_int64(e.score),
				String::num_int64(e.kills),
				String::num_int64(e.level),
				GameData::format_time(e.time),
			};
			for (int c = 0; c < 6; c++) {
				Label *l = ui::label(cells[c], size, c == 2 && i != highlight ? ui::AMBER : col);
				if (c == 1) {
					l->set_h_size_flags(Control::SIZE_EXPAND_FILL);
					l->set_clip_text(true);
				} else if (c >= 2) {
					l->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
				}
				grid->add_child(l);
			}
		}
		if (highlight >= 10) {
			v->add_child(ui::label(U8("Tu última partida quedó en la posición #") + String::num_int64(highlight + 1), 15, ui::CYAN, true));
		}
	}

	v->add_child(memnew(HSeparator));
	Button *back = ui::button(U8("Menú Principal"));
	back->connect("pressed", callable_mp(static_cast<ScreenBase *>(this), &ScreenBase::go_menu));
	v->add_child(back);
	back->grab_focus();
}

// ---------------------------------------------------------------- armeria (tienda incremental)
void ShopScreen::_ready() {
	if (in_editor()) {
		return;
	}
	GameData::setup_input();
	back_scene = SCENE_MENU;
	content = ui::screen_panel(this, 680, 8);
	refresh(-1);
}

void ShopScreen::refresh(int focus_id) {
	for (int i = content->get_child_count() - 1; i >= 0; i--) {
		Node *c = content->get_child(i);
		content->remove_child(c);
		c->queue_free();
	}
	const GameData &gd = GameData::get();

	content->add_child(ui::label(U8("ARMERÍA"), 40, ui::AMBER, true));
	content->add_child(ui::label(U8("Mejoras permanentes: se conservan entre partidas"), 15, ui::MUTED, true));
	content->add_child(ui::label(U8("NÚCLEOS DISPONIBLES:  ") + String::num_int64(gd.cores), 24, Color(1.0f, 0.85f, 0.2f), true));
	content->add_child(memnew(HSeparator));

	Button *to_focus = nullptr;
	for (int id = 0; id < M_COUNT; id++) {
		const MetaUpgradeDef &u = GameData::UPGRADES[id];
		HBoxContainer *row = memnew(HBoxContainer);
		row->add_theme_constant_override("separation", 10);
		VBoxContainer *info = memnew(VBoxContainer);
		info->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		info->add_theme_constant_override("separation", 0);
		info->add_child(ui::label(String::utf8(u.name) + vformat("   [%d/%d]", gd.get_level(id), u.max), 18));
		info->add_child(ui::label(String::utf8(u.desc), 14, ui::MUTED));
		row->add_child(info);

		Button *b = ui::button("", 17, Vector2(170, 42));
		if (gd.is_maxed(id)) {
			b->set_text(U8("MÁXIMO"));
			b->set_disabled(true);
		} else {
			b->set_text(String::num_int64(gd.get_cost(id)) + U8(" núcleos"));
			b->set_disabled(!gd.can_buy(id));
		}
		b->connect("pressed", callable_mp(this, &ShopScreen::on_buy).bind(id));
		row->add_child(b);
		content->add_child(row);
		if (id == focus_id && !b->is_disabled()) {
			to_focus = b;
		}
	}

	content->add_child(memnew(HSeparator));
	Button *back = ui::button(U8("Menú Principal"));
	back->connect("pressed", callable_mp(static_cast<ScreenBase *>(this), &ScreenBase::go_menu));
	content->add_child(back);
	(to_focus != nullptr ? to_focus : back)->grab_focus();
}

void ShopScreen::on_buy(int id) {
	if (GameData::get().buy(id)) {
		refresh(id);
	}
}

} // namespace cc
