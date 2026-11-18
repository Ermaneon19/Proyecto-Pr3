@tool
extends Node3D
## Insecto mecánico (versión 3D del Enemy de SFML): persigue al jugador,
## aletea, mueve las guadañas y parpadea al recibir daño.

const MU = preload("res://scripts/mesh_util.gd")

static var _shared: Dictionary = {}

var main: Node
var alive := true
var hp := 25.0
var max_hp := 25.0
var speed := 3.0
var radius := 0.5
var contact_dps := 10.0
var elite := false
var drone_cd := 0.0

var _t := 0.0
var _flash := 0.0
var _model: Node3D
var _body_parts: Array[MeshInstance3D] = []
var _wing_l: Node3D
var _wing_r: Node3D
var _blade_l: Node3D
var _blade_r: Node3D


static func _get_shared() -> Dictionary:
	if _shared.is_empty():
		_shared = {
			"metal": MU.mat(Color(0.42, 0.44, 0.47), 0.35, 0.4),
			"metal_dark": MU.mat(Color(0.2, 0.21, 0.23), 0.3, 0.5),
			"eye": MU.glow(Color(1.0, 0.45, 0.05), 5.0),
			"blade": MU.glow(Color(1.0, 0.25, 0.75), 3.5),
			"wing": MU.transparent(Color(0.6, 0.85, 0.95, 0.16), 0.3),
			"flash": MU.glow(Color(1, 1, 1), 2.0),
			"shadow": MU.transparent(Color(0, 0, 0, 0.4)),
			"elite_eye": MU.glow(Color(1.0, 0.05, 0.1), 7.0),
			"body": MU.sphere(0.5, 12, 6),
			"head": MU.sphere(0.3, 10, 5),
			"eye_m": MU.sphere(0.09, 6, 3),
			"wing_m": MU.box(Vector3(0.75, 0.01, 0.28)),
			"arm_m": MU.box(Vector3(0.09, 0.09, 0.55)),
			"blade_m": MU.box(Vector3(0.07, 0.07, 0.7)),
			"leg_m": MU.box(Vector3(0.05, 0.6, 0.05)),
			"shadow_m": MU.cylinder(0.6, 0.6, 0.01, 12),
		}
	return _shared


func setup(is_elite: bool, minutes: float) -> void:
	elite = is_elite
	max_hp = 25.0 * (1.0 + minutes * 0.4)
	speed = min(5.6, 2.8 + minutes * 0.25) * randf_range(0.9, 1.1)
	if elite:
		max_hp *= 14.0
		speed *= 0.8
		radius = 0.95
		contact_dps = 30.0
		scale = Vector3.ONE * 1.9
	hp = max_hp


func _ready() -> void:
	_build_model()
	_t = randf() * 10.0


func _build_model() -> void:
	var s := _get_shared()
	MU.part(self, s.shadow_m, s.shadow, Vector3(0, 0.02, 0))
	_model = MU.pivot(self, Vector3(0, 0.0, 0))
	_model.scale = Vector3.ONE * 0.7
	# Cuerpo segmentado (frente = -Z)
	_body_parts.append(MU.part(_model, s.body, s.metal, Vector3(0, 0.95, 0), Vector3.ZERO, Vector3(0.9, 0.75, 1.1)))
	_body_parts.append(MU.part(_model, s.body, s.metal_dark, Vector3(0, 0.85, 0.75), Vector3(-15, 0, 0), Vector3(0.75, 0.65, 1.3)))
	_body_parts.append(MU.part(_model, s.head, s.metal, Vector3(0, 1.1, -0.6), Vector3.ZERO, Vector3(1.0, 0.85, 1.1)))
	# Ojos
	var eye_mat = s.elite_eye if elite else s.eye
	MU.part(_model, s.eye_m, eye_mat, Vector3(-0.13, 1.17, -0.84))
	MU.part(_model, s.eye_m, eye_mat, Vector3(0.13, 1.17, -0.84))
	# Patas
	for side in [-1.0, 1.0]:
		for i in 3:
			MU.part(_model, s.leg_m, s.metal_dark, Vector3(0.45 * side, 0.55, -0.2 + i * 0.35), Vector3(0, 0, 35 * side))
	# Guadañas brillantes
	_blade_l = MU.pivot(_model, Vector3(-0.32, 0.95, -0.45))
	_blade_r = MU.pivot(_model, Vector3(0.32, 0.95, -0.45))
	for b in [_blade_l, _blade_r]:
		MU.part(b, s.arm_m, s.metal, Vector3(0, 0, -0.25), Vector3(20, 0, 0))
		MU.part(b, s.blade_m, s.blade, Vector3(0, -0.3, -0.5), Vector3(-60, 0, 0))
	# Alas
	_wing_l = MU.pivot(_model, Vector3(-0.15, 1.3, 0.1))
	_wing_r = MU.pivot(_model, Vector3(0.15, 1.3, 0.1))
	MU.part(_wing_l, s.wing_m, s.wing, Vector3(-0.38, 0, 0.1), Vector3(0, 20, 0))
	MU.part(_wing_r, s.wing_m, s.wing, Vector3(0.38, 0, 0.1), Vector3(0, -20, 0))


func _process(delta: float) -> void:
	if Engine.is_editor_hint():
		return
	if not alive or main == null or main.player == null:
		return
	_t += delta
	drone_cd = max(0.0, drone_cd - delta)

	var p: Vector3 = main.player.position
	var to := Vector3(p.x - position.x, 0, p.z - position.z)
	var dist := to.length()
	var dir := to / dist if dist > 0.001 else Vector3.ZERO

	# Separación ligera para que no se apilen todos en el mismo punto
	var push := Vector3.ZERO
	for o in main.query(position, radius + 1.0):
		if not is_instance_valid(o) or o == self or not o.alive:
			continue
		var d := Vector3(position.x - o.position.x, 0, position.z - o.position.z)
		var l := d.length()
		var min_d: float = radius + o.radius
		if l < min_d and l > 0.001:
			push += d / l * (min_d - l)
	position += (dir * speed + push * 4.0) * delta

	if dist > 0.01:
		rotation.y = lerp_angle(rotation.y, atan2(-dir.x, -dir.z), 10.0 * delta)

	if dist < radius + 0.45:
		main.player.take_damage(contact_dps * delta)

	# Animación (equivalente a los 6 frames del sprite original)
	_model.position.y = 0.25 + sin(_t * 5.0) * 0.1
	var flap := sin(_t * 40.0) * 0.6
	_wing_l.rotation.z = flap
	_wing_r.rotation.z = -flap
	var slash := sin(_t * 7.0) * 0.5
	_blade_l.rotation.x = slash
	_blade_r.rotation.x = -slash

	if _flash > 0.0:
		_flash -= delta
		if _flash <= 0.0:
			_set_flash(false)


func take_damage(amount: float) -> void:
	if not alive:
		return
	hp -= amount
	main.damage_number(position + Vector3(0, 1.4 * scale.y, 0), amount)
	if _flash <= 0.0:
		_set_flash(true)
	_flash = 0.08
	if hp <= 0.0:
		alive = false
		main.kill_enemy(self)


func _set_flash(on: bool) -> void:
	var s := _get_shared()
	_body_parts[0].material_override = s.flash if on else s.metal
	_body_parts[1].material_override = s.flash if on else s.metal_dark
	_body_parts[2].material_override = s.flash if on else s.metal
