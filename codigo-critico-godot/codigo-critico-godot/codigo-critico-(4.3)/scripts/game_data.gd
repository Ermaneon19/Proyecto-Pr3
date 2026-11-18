extends Node
## Datos persistentes (autoload): moneda, mejoras permanentes y récords.
## Es la parte "incremental": lo que ganas en cada partida se queda para la siguiente.

const SAVE_PATH := "user://save.json"

const UPGRADES := {
	"damage":    {"name": "Munición perforante", "desc": "+10% daño",                  "base_cost": 15, "max": 20},
	"fire_rate": {"name": "Cargador rápido",     "desc": "+8% cadencia de fuego",      "base_cost": 15, "max": 15},
	"max_hp":    {"name": "Blindaje pesado",     "desc": "+20 vida máxima",            "base_cost": 10, "max": 20},
	"speed":     {"name": "Servomotores",        "desc": "+5% velocidad",              "base_cost": 12, "max": 10},
	"magnet":    {"name": "Imán de núcleos",     "desc": "+15% radio de recolección",  "base_cost": 8,  "max": 10},
	"greed":     {"name": "Contrato de minería", "desc": "+10% núcleos obtenidos",     "base_cost": 25, "max": 20},
	"regen":     {"name": "Estimulantes",        "desc": "+0.3 vida por segundo",      "base_cost": 30, "max": 10},
}

var cores: int = 0
var levels: Dictionary = {}
var best_time: float = 0.0
var best_kills: int = 0
var autostart := false
var last_run: Dictionary = {}


func _ready() -> void:
	_setup_input()
	load_game()


func _setup_input() -> void:
	var map := {
		"move_left": [KEY_A, KEY_LEFT],
		"move_right": [KEY_D, KEY_RIGHT],
		"move_up": [KEY_W, KEY_UP],
		"move_down": [KEY_S, KEY_DOWN],
	}
	for action in map:
		if not InputMap.has_action(action):
			InputMap.add_action(action)
		for key in map[action]:
			var ev := InputEventKey.new()
			ev.physical_keycode = key
			InputMap.action_add_event(action, ev)


func get_level(id: String) -> int:
	return int(levels.get(id, 0))


func get_cost(id: String) -> int:
	var base: float = UPGRADES[id]["base_cost"]
	return int(round(base * pow(1.6, get_level(id))))


func is_maxed(id: String) -> bool:
	return get_level(id) >= int(UPGRADES[id]["max"])


func can_buy(id: String) -> bool:
	return not is_maxed(id) and cores >= get_cost(id)


func buy(id: String) -> bool:
	if not can_buy(id):
		return false
	cores -= get_cost(id)
	levels[id] = get_level(id) + 1
	save_game()
	return true


func add_run_result(earned: int, time_s: float, kills: int) -> void:
	cores += earned
	best_time = max(best_time, time_s)
	best_kills = max(best_kills, kills)
	last_run = {"earned": earned, "time": time_s, "kills": kills}
	save_game()


func save_game() -> void:
	var f := FileAccess.open(SAVE_PATH, FileAccess.WRITE)
	if f == null:
		return
	f.store_string(JSON.stringify({
		"cores": cores, "levels": levels,
		"best_time": best_time, "best_kills": best_kills,
	}))


func load_game() -> void:
	if not FileAccess.file_exists(SAVE_PATH):
		return
	var f := FileAccess.open(SAVE_PATH, FileAccess.READ)
	var data = JSON.parse_string(f.get_as_text())
	if typeof(data) != TYPE_DICTIONARY:
		return
	cores = int(data.get("cores", 0))
	best_time = float(data.get("best_time", 0.0))
	best_kills = int(data.get("best_kills", 0))
	levels = {}
	var lv = data.get("levels", {})
	if typeof(lv) == TYPE_DICTIONARY:
		for k in lv:
			levels[k] = int(lv[k])
