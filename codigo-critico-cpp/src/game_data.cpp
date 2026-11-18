#include "game_data.h"
#include "util.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace cc {

static const char *SAVE_PATH = "user://save.json";
static const char *RANKING_PATH = "user://ranking.json";

const MetaUpgradeDef GameData::UPGRADES[M_COUNT] = {
	{ "damage", "Munición perforante", "+10% daño", 15, 20 },
	{ "fire_rate", "Cargador rápido", "+8% cadencia de fuego", 15, 15 },
	{ "max_hp", "Blindaje pesado", "+20 vida máxima", 10, 20 },
	{ "speed", "Servomotores", "+5% velocidad", 12, 10 },
	{ "magnet", "Imán de núcleos", "+15% radio de recolección", 8, 10 },
	{ "greed", "Contrato de minería", "+10% núcleos obtenidos", 25, 20 },
	{ "regen", "Estimulantes", "+0.3 vida por segundo", 30, 10 },
};

GameData &GameData::get() {
	static GameData instance;
	static bool loaded = false;
	if (!loaded) {
		loaded = true;
		instance.load_game();
		instance.load_ranking();
	}
	return instance;
}

int GameData::get_cost(int id) const {
	return int(std::round(UPGRADES[id].base_cost * std::pow(1.6, double(levels[id]))));
}

bool GameData::is_maxed(int id) const {
	return levels[id] >= UPGRADES[id].max;
}

bool GameData::can_buy(int id) const {
	return !is_maxed(id) && cores >= get_cost(id);
}

bool GameData::buy(int id) {
	if (!can_buy(id)) {
		return false;
	}
	cores -= get_cost(id);
	levels[id] += 1;
	save_game();
	return true;
}

void GameData::add_run_result(int score, int earned, float time_s, int kills, int level) {
	cores += earned;
	best_time = std::max(best_time, time_s);
	best_kills = std::max(best_kills, kills);
	best_score = std::max(best_score, score);

	RankEntry e;
	e.name = player_name.strip_edges();
	if (e.name.is_empty()) {
		e.name = U8("Anónimo");
	}
	e.score = score;
	e.kills = kills;
	e.level = level;
	e.time = time_s;

	// Insercion ordenada: los empates quedan debajo de los puntajes anteriores.
	size_t pos = 0;
	while (pos < ranking.size() && ranking[pos].score >= score) {
		pos++;
	}
	ranking.insert(ranking.begin() + pos, e);
	if (int(ranking.size()) > MAX_RANKING) {
		ranking.resize(MAX_RANKING);
	}

	last_run.valid = true;
	last_run.name = e.name;
	last_run.score = score;
	last_run.kills = kills;
	last_run.level = level;
	last_run.earned = earned;
	last_run.time = time_s;
	last_run.rank = int(pos) + 1;

	save_game();
	save_ranking();
}

void GameData::save_game() const {
	Ref<FileAccess> f = FileAccess::open(SAVE_PATH, FileAccess::WRITE);
	if (f.is_null()) {
		return;
	}
	Dictionary lv;
	for (int i = 0; i < M_COUNT; i++) {
		lv[String(UPGRADES[i].id)] = levels[i];
	}
	Dictionary d;
	d["cores"] = cores;
	d["levels"] = lv;
	d["best_time"] = best_time;
	d["best_kills"] = best_kills;
	d["best_score"] = best_score;
	d["last_name"] = player_name;
	f->store_string(JSON::stringify(d));
}

void GameData::load_game() {
	if (!FileAccess::file_exists(SAVE_PATH)) {
		return;
	}
	Ref<FileAccess> f = FileAccess::open(SAVE_PATH, FileAccess::READ);
	if (f.is_null()) {
		return;
	}
	Variant data = JSON::parse_string(f->get_as_text());
	if (data.get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary d = data;
	cores = int(d.get("cores", 0));
	best_time = float(d.get("best_time", 0.0));
	best_kills = int(d.get("best_kills", 0));
	best_score = int(d.get("best_score", 0));
	player_name = String(d.get("last_name", ""));
	Variant lvv = d.get("levels", Dictionary());
	if (lvv.get_type() == Variant::DICTIONARY) {
		Dictionary lv = lvv;
		for (int i = 0; i < M_COUNT; i++) {
			int v = int(lv.get(String(UPGRADES[i].id), 0));
			levels[i] = std::clamp(v, 0, UPGRADES[i].max);
		}
	}
}

void GameData::save_ranking() const {
	Ref<FileAccess> f = FileAccess::open(RANKING_PATH, FileAccess::WRITE);
	if (f.is_null()) {
		return;
	}
	Array arr;
	for (const RankEntry &e : ranking) {
		Dictionary d;
		d["name"] = e.name;
		d["score"] = e.score;
		d["kills"] = e.kills;
		d["level"] = e.level;
		d["time"] = e.time;
		arr.push_back(d);
	}
	f->store_string(JSON::stringify(arr, "\t"));
}

void GameData::load_ranking() {
	ranking.clear();
	if (!FileAccess::file_exists(RANKING_PATH)) {
		return;
	}
	Ref<FileAccess> f = FileAccess::open(RANKING_PATH, FileAccess::READ);
	if (f.is_null()) {
		return;
	}
	Variant data = JSON::parse_string(f->get_as_text());
	if (data.get_type() != Variant::ARRAY) {
		return;
	}
	Array arr = data;
	for (int i = 0; i < arr.size(); i++) {
		Variant v = arr[i];
		if (v.get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary d = v;
		RankEntry e;
		e.name = String(d.get("name", "?"));
		e.score = int(d.get("score", 0));
		e.kills = int(d.get("kills", 0));
		e.level = int(d.get("level", 1));
		e.time = float(d.get("time", 0.0));
		ranking.push_back(e);
	}
	std::stable_sort(ranking.begin(), ranking.end(),
			[](const RankEntry &a, const RankEntry &b) { return a.score > b.score; });
	if (int(ranking.size()) > MAX_RANKING) {
		ranking.resize(MAX_RANKING);
	}
}

void GameData::setup_input() {
	static bool done = false;
	if (done) {
		return;
	}
	done = true;
	struct Bind {
		const char *action;
		Key keys[2];
	};
	static const Bind binds[] = {
		{ "move_left", { KEY_A, KEY_LEFT } },
		{ "move_right", { KEY_D, KEY_RIGHT } },
		{ "move_up", { KEY_W, KEY_UP } },
		{ "move_down", { KEY_S, KEY_DOWN } },
		{ "dash", { KEY_SPACE, KEY_SHIFT } },
		{ "pause", { KEY_ESCAPE, KEY_P } },
	};
	InputMap *im = InputMap::get_singleton();
	for (const Bind &b : binds) {
		StringName action(b.action);
		if (!im->has_action(action)) {
			im->add_action(action);
		}
		for (Key k : b.keys) {
			Ref<InputEventKey> ev;
			ev.instantiate();
			ev->set_physical_keycode(k);
			im->action_add_event(action, ev);
		}
	}
}

String GameData::format_time(float seconds) {
	int t = int(seconds);
	return vformat("%02d:%02d", t / 60, t % 60);
}

} // namespace cc
