// Datos persistentes: moneda, mejoras permanentes, jugador registrado y ranking.
// Es la parte "incremental": lo que ganas en cada partida se queda para la siguiente.
#pragma once

#include <godot_cpp/variant/string.hpp>

#include <map.hpp>

#include <cstddef>
#include <vector>
#include <string>

namespace cc {

enum MetaUpgrade {
	M_DAMAGE,
	M_FIRE_RATE,
	M_MAX_HP,
	M_SPEED,
	M_MAGNET,
	M_GREED,
	M_REGEN,
	M_COUNT
};

struct MetaUpgradeDef {
	const char *id;
	const char *name;
	const char *desc;
	int base_cost;
	int max;
};

struct RankEntry {
	godot::String name;
	int score = 0;
	int kills = 0;
	int level = 1;
	float time = 0.0f;
};

struct RunResult {
	bool valid = false;
	godot::String name;
	int score = 0;
	int kills = 0;
	int level = 1;
	int earned = 0;
	int rank = 0; // posicion (1 = primero) en el ranking
	float time = 0.0f;
};

struct PlayerStats {
	godot::String name;
	int games_played = 0;
	int best_score = 0;
	int total_kills = 0;
};

class GameData {
public:
	static const MetaUpgradeDef UPGRADES[M_COUNT];
	static const int MAX_RANKING = 50;

	static GameData &get(); // carga perezosa desde user://

	int cores = 0;
	int levels[M_COUNT] = {};
	float best_time = 0.0f;
	int best_kills = 0;
	int best_score = 0;

	godot::String player_name; // nombre/alias registrado para la partida actual
	RunResult last_run;
	std::vector<RankEntry> ranking; // ordenado de mayor a menor puntaje
	std::vector<PlayerStats> player_stats;
	Designar::HashMap<std::string, size_t> player_stats_index;

	int get_level(int id) const { return levels[id]; }
	int get_cost(int id) const;
	bool is_maxed(int id) const;
	bool can_buy(int id) const;
	bool buy(int id);

	// Empareja el nombre registrado con el puntaje final y lo guarda en el ranking.
	void add_run_result(int score, int earned, float time_s, int kills, int level);
	const PlayerStats *find_player_stats(const godot::String &name) const;

	void save_game() const;
	void save_ranking() const;

	static void setup_input();
	static godot::String format_time(float seconds);

private:
	void load_game();
	void load_ranking();
};

} // namespace cc
