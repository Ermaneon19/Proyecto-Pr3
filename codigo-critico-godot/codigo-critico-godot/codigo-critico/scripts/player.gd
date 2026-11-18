@tool
extends Node3D
## Soldado: movimiento WASD, rifle con auto-apuntado, bombardeo orbital y drones de escolta.

const MU = preload("res://scripts/mesh_util.gd")

signal died

var main: Node  # referencia a main.gd (la asigna main)
var active := false
var alive := true

# --- Estadísticas (base + mejoras permanentes de GameData) ---
var max_hp := 100.0
var hp := 100.0
var speed := 7.0
var damage := 10.0
var fire_interval := 0.45
var projectiles := 1
var pierce := 0
var magnet_radius := 3.0
var regen := 0.0
var core_mult := 1.0

# --- Armas desbloqueables durante la partida ---
var rifle_level := 1
var strike_level := 0
var drone_level := 0

var _fire_timer := 0.0
var _strike_timer := 2.0
var _target_refresh_timer := 0.0
var _target: Node3D
var _anim_t := 0.0
var _hurt_flash := 0.0
var _drone_angle := 0.0
var _drones: Array[Node3D] = []

var _model: Node3D
var _leg_l: Node3D
var _leg_r: Node3D
var _arm_l: Node3D
var _cape: Node3D
var _muzzle: Node3D
var _armor_mat: StandardMaterial3D


func _ready() -> void:
	if Engine.is_editor_hint():
		_build_model()
		return
	_apply_meta()
	hp = max_hp
	_build_model()


func _apply_meta() -> void:
	damage = 10.0 * (1.0 + 0.10 * GameData.get_level("damage"))
	fire_interval = 0.45 / (1.0 + 0.08 * GameData.get_level("fire_rate"))
	max_hp = 100.0 + 20.0 * GameData.get_level("max_hp")
	speed = 7.0 * (1.0 + 0.05 * GameData.get_level("speed"))
	magnet_radius = 3.0 * (1.0 + 0.15 * GameData.get_level("magnet"))
	core_mult = 1.0 + 0.10 * GameData.get_level("greed")
	regen = 0.3 * GameData.get_level("regen")


# ---------------------------------------------------------------- modelo
func _build_model() -> void:
	_model = MU.pivot(self)
	_armor_mat = MU.mat(Color(0.48, 0.5, 0.47), 0.25, 0.5)
	var dark := MU.mat(Color(0.16, 0.17, 0.19), 0.2, 0.6)
	var accent := MU.mat(Color(0.95, 0.62, 0.12), 0.4, 0.4)
	var cape_mat := MU.mat(Color(0.14, 0.15, 0.18), 0.0, 0.9)
	cape_mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	var visor := MU.glow(Color(0.25, 0.85, 1.0), 2.5)

	# Piernas (pivotan en la cadera para animar la caminata)
	_leg_l = MU.pivot(_model, Vector3(-0.2, 0.9, 0))
	_leg_r = MU.pivot(_model, Vector3(0.2, 0.9, 0))
	for leg in [_leg_l, _leg_r]:
		MU.part(leg, MU.box(Vector3(0.28, 0.75, 0.3)), _armor_mat, Vector3(0, -0.38, 0))
		MU.part(leg, MU.box(Vector3(0.32, 0.18, 0.42)), dark, Vector3(0, -0.82, -0.05))
		MU.part(leg, MU.box(Vector3(0.3, 0.16, 0.08)), accent, Vector3(0, -0.35, -0.17))

	# Torso y armadura
	MU.part(_model, MU.box(Vector3(0.7, 0.3, 0.42)), dark, Vector3(0, 0.98, 0))
	MU.part(_model, MU.box(Vector3(0.82, 0.62, 0.48)), _armor_mat, Vector3(0, 1.38, 0))
	MU.part(_model, MU.box(Vector3(0.5, 0.08, 0.02)), accent, Vector3(0, 1.5, -0.25))
	MU.part(_model, MU.box(Vector3(0.6, 0.55, 0.3)), dark, Vector3(0, 1.4, 0.36))  # mochila
	MU.part(_model, MU.cylinder(0.06, 0.06, 0.5), accent, Vector3(0.22, 1.75, 0.4))  # antena

	# Hombreras
	for s in [-1.0, 1.0]:
		MU.part(_model, MU.box(Vector3(0.36, 0.2, 0.5)), _armor_mat, Vector3(0.52 * s, 1.68, 0), Vector3(0, 0, -12 * s))
		MU.part(_model, MU.box(Vector3(0.37, 0.05, 0.51)), accent, Vector3(0.52 * s, 1.6, 0), Vector3(0, 0, -12 * s))

	# Brazos: el derecho sostiene el rifle
	_arm_l = MU.pivot(_model, Vector3(-0.5, 1.6, 0))
	MU.part(_arm_l, MU.box(Vector3(0.2, 0.55, 0.22)), dark, Vector3(0, -0.3, -0.1), Vector3(-35, 0, 0))
	var arm_r := MU.pivot(_model, Vector3(0.5, 1.6, 0))
	MU.part(arm_r, MU.box(Vector3(0.2, 0.55, 0.22)), dark, Vector3(0, -0.3, -0.1), Vector3(-50, 0, 0))

	# Rifle
	var gun := MU.pivot(_model, Vector3(0.28, 1.25, -0.45))
	MU.part(gun, MU.box(Vector3(0.13, 0.16, 0.95)), dark)
	MU.part(gun, MU.box(Vector3(0.1, 0.22, 0.12)), dark, Vector3(0, -0.15, 0.1))
	MU.part(gun, MU.box(Vector3(0.14, 0.04, 0.5)), accent, Vector3(0, 0.1, -0.05))
	_muzzle = MU.pivot(gun, Vector3(0, 0, -0.55))

	# Casco con visor
	MU.part(_model, MU.sphere(0.3), _armor_mat, Vector3(0, 1.95, 0), Vector3.ZERO, Vector3(1, 0.95, 1.05))
	MU.part(_model, MU.box(Vector3(0.44, 0.14, 0.2)), dark, Vector3(0, 1.93, -0.2))
	MU.part(_model, MU.box(Vector3(0.36, 0.06, 0.05)), visor, Vector3(0, 1.94, -0.31))
	MU.part(_model, MU.box(Vector3(0.06, 0.08, 0.5)), accent, Vector3(0, 2.22, 0))  # cresta

	# Capa (pivota en los hombros)
	_cape = MU.pivot(_model, Vector3(0, 1.68, 0.3))
	MU.part(_cape, MU.box(Vector3(0.8, 1.15, 0.03)), cape_mat, Vector3(0, -0.58, 0))
	MU.part(_cape, MU.box(Vector3(0.81, 0.08, 0.035)), accent, Vector3(0, -1.08, 0))

	# Sombra falsa (disco oscuro bajo los pies)
	var shadow := MU.transparent(Color(0, 0, 0, 0.45))
	MU.part(self, MU.cylinder(0.6, 0.6, 0.01, 20), shadow, Vector3(0, 0.02, 0))


# ---------------------------------------------------------------- bucle
func _process(delta: float) -> void:
	if Engine.is_editor_hint():
		return
	_anim_t += delta
	_target_refresh_timer -= delta
	if _target_refresh_timer <= 0.0:
		_target = main.nearest_enemy(position, 18.0) if main else null
		_target_refresh_timer = 0.1
	var dir := Vector3.ZERO
	if active and alive:
		var input := Input.get_vector("move_left", "move_right", "move_up", "move_down")
		dir = Vector3(input.x, 0, input.y)
		position += dir * speed * delta
		hp = min(max_hp, hp + regen * delta)
		_update_weapons(delta)

	_animate(dir, delta)


func _animate(dir: Vector3, delta: float) -> void:
	var moving := dir.length() > 0.1
	var swing := sin(_anim_t * 12.0) * 0.6 if moving else 0.0
	_leg_l.rotation.x = lerp(_leg_l.rotation.x, swing, 12.0 * delta)
	_leg_r.rotation.x = lerp(_leg_r.rotation.x, -swing, 12.0 * delta)
	_arm_l.rotation.x = lerp(_arm_l.rotation.x, -swing * 0.5, 12.0 * delta)
	var cape_target := 0.45 + sin(_anim_t * 9.0) * 0.08 if moving else 0.08 + sin(_anim_t * 2.0) * 0.04
	_cape.rotation.x = lerp(_cape.rotation.x, cape_target, 6.0 * delta)
	_model.position.y = abs(sin(_anim_t * 12.0)) * 0.06 if moving else 0.0

	# Mira hacia el enemigo más cercano; si no hay, hacia donde camina
	var look := Vector3.ZERO
	if is_instance_valid(_target):
		look = _target.position - position
	elif moving:
		look = dir
	if look.length() > 0.01:
		var ang := atan2(-look.x, -look.z)
		_model.rotation.y = lerp_angle(_model.rotation.y, ang, 14.0 * delta)

	if _hurt_flash > 0.0:
		_hurt_flash -= delta
		_armor_mat.emission_enabled = true
		_armor_mat.emission = Color(1, 0.1, 0.1)
		_armor_mat.emission_energy_multiplier = _hurt_flash * 6.0
	else:
		_armor_mat.emission_enabled = false


# ---------------------------------------------------------------- armas
func _update_weapons(delta: float) -> void:
	# Rifle
	_fire_timer -= delta
	if _fire_timer <= 0.0:
		if is_instance_valid(_target):
			_fire_timer = fire_interval
			var base_dir: Vector3 = (_target.position - position)
			base_dir.y = 0
			base_dir = base_dir.normalized()
			var spread := 0.14
			for i in projectiles:
				var off := (i - (projectiles - 1) / 2.0) * spread
				var d := base_dir.rotated(Vector3.UP, off)
				var from := _muzzle.global_position
				from.y = 1.25
				main.spawn_bullet(from, d, damage, pierce)

	# Bombardeo orbital
	if strike_level > 0:
		_strike_timer -= delta
		if _strike_timer <= 0.0:
			_strike_timer = max(1.5, 6.0 - strike_level * 0.5)
			var bursts := 1 + strike_level / 3
			for i in bursts:
				var t = main.random_enemy_near(position, 16.0)
				var pos: Vector3 = t.position if t else position + Vector3(randf_range(-8, 8), 0, randf_range(-8, 8))
				main.orbital_strike(pos, 2.6 + strike_level * 0.35, damage * (2.5 + strike_level * 0.6))

	# Drones de escolta
	if _drones.size() > 0:
		_drone_angle += delta * (2.4 + drone_level * 0.2)
		var radius := 2.6 + drone_level * 0.15
		for i in _drones.size():
			var a := _drone_angle + TAU * i / _drones.size()
			var dpos := position + Vector3(cos(a) * radius, 1.2, sin(a) * radius)
			_drones[i].position = dpos
			_drones[i].rotation.y += delta * 8.0
			for e in main.query(dpos, 1.5):
				if e.alive and e.drone_cd <= 0.0 and Vector2(e.position.x - dpos.x, e.position.z - dpos.z).length() < 0.6 + e.radius:
					e.drone_cd = 0.45
					e.take_damage(damage * (0.7 + drone_level * 0.15))


func _add_drone() -> void:
	var d := Node3D.new()
	MU.part(d, MU.sphere(0.22, 8, 4), MU.mat(Color(0.2, 0.22, 0.25), 0.8, 0.3))
	MU.part(d, MU.box(Vector3(0.7, 0.04, 0.12)), MU.glow(Color(0.3, 0.9, 1.0), 3.0))
	MU.part(d, MU.box(Vector3(0.12, 0.04, 0.7)), MU.glow(Color(0.3, 0.9, 1.0), 3.0))
	main.world.add_child(d)
	_drones.append(d)


# ---------------------------------------------------------------- vida / mejoras
func take_damage(amount: float) -> void:
	if not alive or not active:
		return
	hp -= amount
	_hurt_flash = 0.25
	if hp <= 0.0:
		hp = 0.0
		alive = false
		died.emit()


func heal(amount: float) -> void:
	hp = min(max_hp, hp + amount)


func apply_upgrade(id: String) -> void:
	match id:
		"dmg":
			damage *= 1.2
			rifle_level += 1
		"rate":
			fire_interval /= 1.15
			rifle_level += 1
		"multi":
			projectiles += 1
			rifle_level += 1
		"pierce":
			pierce += 1
			rifle_level += 1
		"speed":
			speed *= 1.1
		"hp":
			max_hp += 25.0
			heal(25.0)
		"magnet":
			magnet_radius *= 1.3
		"strike":
			strike_level += 1
		"drones":
			drone_level += 1
			_add_drone()
		"heal":
			heal(max_hp * 0.5)
