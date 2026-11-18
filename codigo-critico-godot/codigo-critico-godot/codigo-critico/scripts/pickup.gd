extends Node3D
## Cristal de experiencia (cian) o Núcleo (amarillo, moneda permanente).
## Equivalente al Core de SFML, pero con imán como en Vampire Survivors.

const MU = preload("res://scripts/mesh_util.gd")
static var _cache: Dictionary = {}

var main: Node
var kind := "xp"   # "xp" | "core"
var value := 1.0
var _attracted := false
var _vel := 0.0
var _t := 0.0
var _visual: Node3D


func _ready() -> void:
	if _cache.is_empty():
		_cache = {
			"xp_m": MU.sphere(0.18, 4, 2),
			"xp_big_m": MU.sphere(0.3, 4, 2),
			"core_m": MU.sphere(0.22, 10, 5),
			"xp": MU.glow(Color(0.25, 0.7, 1.0), 2.5),
			"xp_big": MU.glow(Color(0.9, 0.3, 1.0), 3.0),
			"core": MU.glow(Color(1.0, 0.85, 0.15), 4.0),
		}
	_visual = MU.pivot(self, Vector3(0, 0.45, 0))
	if kind == "core":
		MU.part(_visual, _cache.core_m, _cache.core)
	elif value >= 5:
		MU.part(_visual, _cache.xp_big_m, _cache.xp_big, Vector3.ZERO, Vector3.ZERO, Vector3(1, 1.6, 1))
	else:
		MU.part(_visual, _cache.xp_m, _cache.xp, Vector3.ZERO, Vector3.ZERO, Vector3(1, 1.6, 1))
	_t = randf() * 6.0


func _process(delta: float) -> void:
	_t += delta
	_visual.rotation.y += delta * 3.0
	_visual.position.y = 0.45 + sin(_t * 4.0) * 0.08
	var player = main.player
	if player == null or not player.alive:
		return
	var to: Vector3 = player.position - position
	to.y = 0
	var d := to.length()
	if not _attracted and d < player.magnet_radius:
		_attracted = true
	if _attracted:
		_vel = min(_vel + 40.0 * delta, 25.0)
		position += to.normalized() * _vel * delta
	if d < 0.6:
		main.collect(self)
		queue_free()
