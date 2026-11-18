@tool
extends Node3D
## Orquestador de la partida (equivale a Game.cpp): mundo, cámara, oleadas,
## colisiones (grid espacial), experiencia/niveles y fin de partida.

const MU = preload("res://scripts/mesh_util.gd")
const PlayerScript = preload("res://scripts/player.gd")
const EnemyScript = preload("res://scripts/enemy.gd")
const BulletScript = preload("res://scripts/bullet.gd")
const PickupScript = preload("res://scripts/pickup.gd")
const HUDScript = preload("res://scripts/hud.gd")

const CELL := 2.0
const MAX_ENEMIES := 180
const GRID_REFRESH_INTERVAL := 0.1
const DAMAGE_LABEL_POOL_SIZE := 8
const MAX_DEATH_FX := 8
const CAM_OFFSET := Vector3(0, 15, 10)

static var _death_fx_mesh: SphereMesh
static var _death_fx_material: Material

const LEVELUP_POOL := [
	{"id": "dmg",    "title": "Calibre mayor",      "desc": "+20% daño de todas las armas", "max": 99},
	{"id": "rate",   "title": "Gatillo ligero",     "desc": "+15% cadencia del rifle",      "max": 10},
	{"id": "multi",  "title": "Ráfaga",             "desc": "+1 proyectil por disparo",     "max": 5},
	{"id": "pierce", "title": "Perforación",        "desc": "Las balas atraviesan +1",      "max": 5},
	{"id": "speed",  "title": "Servo-piernas",      "desc": "+10% velocidad",               "max": 5},
	{"id": "hp",     "title": "Placas de blindaje", "desc": "+25 vida máxima",              "max": 99},
	{"id": "magnet", "title": "Imán",               "desc": "+30% radio de recolección",    "max": 5},
	{"id": "strike", "title": "Bombardeo orbital",  "desc": "Explosiones desde la órbita",  "max": 8},
	{"id": "drones", "title": "Drones de escolta",  "desc": "+1 dron que gira a tu alrededor", "max": 6},
]

var world: Node3D
var player: Node3D
var camera: Camera3D
var hud: CanvasLayer
var ground: MeshInstance3D

var enemies: Array = []
var state := "menu"   # menu | playing | levelup | dead
var run_time := 0.0
var kills := 0
var level := 1
var xp := 0.0
var xp_next := 4.0
var run_cores := 0.0
var taken: Dictionary = {}
var pending_levels := 0

var _grid: Dictionary = {}
var _grid_refresh_timer := 0.0
var _spawn_timer := 1.0
var _next_elite := 60.0
var _shake := 0.0
var _damage_labels: Array[Label3D] = []
var _damage_label_ages: Array[float] = []
var _dmg_label_timer := 0.0
var _death_fx_active := 0


func _ready() -> void:
	if Engine.is_editor_hint():
		if has_node("World"):
			return
		_build_world()
		player = PlayerScript.new()
		player.name = "PreviewPlayer"
		player.main = self
		world.add_child(player)
		var preview_positions := [
			Vector3(-4, 0, -3),
			Vector3(4, 0, -4),
			Vector3(-5, 0, 4),
			Vector3(5, 0, 3),
		]
		for i in preview_positions.size():
			var enemy = EnemyScript.new()
			enemy.name = "PreviewEnemy%d" % (i + 1)
			enemy.main = self
			enemy.setup(false, 0.0)
			enemy.position = preview_positions[i]
			world.add_child(enemy)
		return

	randomize()
	_build_world()
	_init_damage_labels()
	player = PlayerScript.new()
	player.main = self
	world.add_child(player)
	player.died.connect(_on_player_died)

	hud = HUDScript.new()
	hud.main = self
	add_child(hud)
	hud.deploy_pressed.connect(start_run)
	hud.upgrade_chosen.connect(_on_upgrade_chosen)

	if GameData.autostart:
		GameData.autostart = false
		start_run()
	else:
		hud.show_base()


func _build_world() -> void:
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.02, 0.03, 0.035)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.45, 0.5, 0.55)
	env.ambient_light_energy = 0.5
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.glow_enabled = false
	env.fog_enabled = false
	env.fog_light_color = Color(0.03, 0.04, 0.045)
	env.fog_density = 0.012
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)

	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-55, -35, 0)
	sun.light_energy = 1.1
	sun.light_color = Color(1.0, 0.92, 0.82)
	sun.shadow_enabled = false
	sun.directional_shadow_max_distance = 45.0
	add_child(sun)

	world = Node3D.new()
	world.name = "World"
	add_child(world)

	# Suelo infinito: un plano que sigue al jugador con un shader de cuadrícula en coordenadas de mundo
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
varying vec3 wpos;
void vertex() { wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }
void fragment() {
	vec2 uv = wpos.xz / 4.0;
	vec2 g = abs(fract(uv - 0.5) - 0.5) / fwidth(uv);
	float line = 1.0 - min(min(g.x, g.y), 1.0);
	vec2 cell = floor(uv);
	float n = fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453);
	vec3 base = mix(vec3(0.06, 0.075, 0.07), vec3(0.085, 0.1, 0.09), n);
	ALBEDO = mix(base, vec3(0.16, 0.2, 0.18), line * 0.8);
	ROUGHNESS = 0.95;
}
"""
	var gm := ShaderMaterial.new()
	gm.shader = shader
	var plane := PlaneMesh.new()
	plane.size = Vector2(240, 240)
	ground = MU.part(self, plane, gm)

	# Rocas/escombros decorativos con MultiMesh (muy barato de dibujar)
	var rock := MU.box(Vector3(1, 1, 1))
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = rock
	mm.instance_count = 260
	for i in mm.instance_count:
		var p := Vector3(randf_range(-120, 120), 0, randf_range(-120, 120))
		if p.length() < 6:
			p += Vector3(10, 0, 10)
		var s := Vector3(randf_range(0.4, 2.2), randf_range(0.2, 1.4), randf_range(0.4, 2.2))
		var b := Basis.from_euler(Vector3(randf_range(-0.2, 0.2), randf() * TAU, randf_range(-0.2, 0.2))).scaled(s)
		mm.set_instance_transform(i, Transform3D(b, p + Vector3(0, s.y * 0.3, 0)))
	var mmi := MultiMeshInstance3D.new()
	mmi.multimesh = mm
	mmi.material_override = MU.mat(Color(0.13, 0.14, 0.14), 0.2, 0.9)
	add_child(mmi)

	camera = Camera3D.new()
	camera.fov = 50
	camera.far = 200
	add_child(camera)
	camera.position = CAM_OFFSET
	camera.look_at(Vector3(0, 1, 0))


# ---------------------------------------------------------------- partida
func start_run() -> void:
	if state == "dead":
		get_tree().paused = false
		GameData.autostart = true
		get_tree().reload_current_scene()
		return
	state = "playing"
	player.active = true
	hud.hide_base()


func _process(delta: float) -> void:
	if Engine.is_editor_hint():
		return
	_dmg_label_timer = maxf(0.0, _dmg_label_timer - delta)
	_update_damage_labels(delta)
	_grid_refresh_timer -= delta
	if _grid_refresh_timer <= 0.0:
		_rebuild_grid()
		_grid_refresh_timer = GRID_REFRESH_INTERVAL
	if player:
		var target := player.position + CAM_OFFSET
		camera.position = camera.position.lerp(target, min(1.0, 8.0 * delta))
		if _shake > 0.0:
			_shake = max(0.0, _shake - delta * 2.5)
			camera.position += Vector3(randf_range(-1, 1), randf_range(-1, 1), randf_range(-1, 1)) * _shake * 0.5
		ground.position = Vector3(snapped(player.position.x, 4.0), 0, snapped(player.position.z, 4.0))

	if state != "playing":
		return
	run_time += delta
	_spawn_tick(delta)


func _spawn_tick(delta: float) -> void:
	var minutes := run_time / 60.0
	_spawn_timer -= delta
	if _spawn_timer <= 0.0:
		_spawn_timer = max(0.25, 1.0 - minutes * 0.15)
		var count := 1 + int(minutes * 1.5)
		for i in count:
			if enemies.size() < MAX_ENEMIES:
				_spawn_enemy(false, minutes)
	if run_time >= _next_elite:
		_next_elite += 60.0
		_spawn_enemy(true, minutes)
		hud.toast("¡Se acerca un insecto élite!")


func _spawn_enemy(elite: bool, minutes: float) -> void:
	var e := EnemyScript.new()
	e.main = self
	e.setup(elite, minutes)
	var a := randf() * TAU
	var r := randf_range(22.0, 27.0)
	e.position = player.position + Vector3(cos(a) * r, 0, sin(a) * r)
	world.add_child(e)
	enemies.append(e)


func kill_enemy(e: Node3D) -> void:
	enemies.erase(e)
	kills += 1
	var pos: Vector3 = e.position
	if e.elite:
		_spawn_pickup(pos, "xp", 10.0)
		for i in 8:
			_spawn_pickup(pos + Vector3(randf_range(-1.5, 1.5), 0, randf_range(-1.5, 1.5)), "core", 1.0)
		_shake = 0.8
	else:
		_spawn_pickup(pos, "xp", 1.0)
		if randf() < 0.12:
			_spawn_pickup(pos + Vector3(0.4, 0, 0.3), "core", 1.0)
	_death_burst(pos, e.scale.x)
	e.queue_free()


func _spawn_pickup(pos: Vector3, kind: String, value: float) -> void:
	var p := PickupScript.new()
	p.main = self
	p.kind = kind
	p.value = value
	p.position = Vector3(pos.x, 0, pos.z)
	world.add_child(p)


func collect(p: Node3D) -> void:
	if p.kind == "core":
		run_cores += p.value * player.core_mult
		return
	xp += p.value
	while xp >= xp_next:
		xp -= xp_next
		level += 1
		xp_next = 4.0 + (level - 1) * 5.0
		pending_levels += 1
	if pending_levels > 0 and state == "playing":
		_open_levelup()


func _open_levelup() -> void:
	var options := []
	for o in LEVELUP_POOL:
		if int(taken.get(o.id, 0)) < int(o.max):
			options.append(o)
	options.shuffle()
	options = options.slice(0, 3)
	if options.is_empty():
		options = [{"id": "heal", "title": "Botiquín", "desc": "Recupera 50% de vida", "max": 99}]
	state = "levelup"
	get_tree().paused = true
	hud.show_levelup(options)


func _on_upgrade_chosen(id: String) -> void:
	taken[id] = int(taken.get(id, 0)) + 1
	player.apply_upgrade(id)
	pending_levels -= 1
	get_tree().paused = false
	state = "playing"
	if pending_levels > 0:
		_open_levelup()


func _on_player_died() -> void:
	state = "dead"
	var earned := int(floor(run_cores)) + kills / 25
	GameData.add_run_result(earned, run_time, kills)
	_shake = 1.0
	await get_tree().create_timer(1.2).timeout
	get_tree().paused = true
	hud.show_base()


func level_of(id: String) -> int:
	return int(taken.get(id, 0))


# ---------------------------------------------------------------- armas / efectos
func spawn_bullet(from: Vector3, dir: Vector3, dmg: float, pierce: int) -> void:
	var b := BulletScript.new()
	b.main = self
	b.position = from
	b.velocity = dir * 26.0
	b.damage = dmg
	b.pierce_left = pierce
	world.add_child(b)


func orbital_strike(pos: Vector3, radius: float, dmg: float) -> void:
	var marker := Node3D.new()
	marker.position = Vector3(pos.x, 0, pos.z)
	world.add_child(marker)
	MU.part(marker, MU.cylinder(radius, radius, 0.02, 32), MU.transparent(Color(1, 0.2, 0.1, 0.25), 1.0), Vector3(0, 0.05, 0))
	var beam := MU.part(marker, MU.cylinder(0.12, 0.12, 30, 8), MU.transparent(Color(1, 0.25, 0.1, 0.6), 4.0), Vector3(0, 15, 0))
	var tw := marker.create_tween()
	tw.tween_property(beam, "scale", Vector3(4, 1, 4), 0.7)
	tw.tween_callback(func():
		_explode(marker.position, radius, dmg)
		marker.queue_free())


func _explode(pos: Vector3, radius: float, dmg: float) -> void:
	for e in query(pos, radius + 1.5):
		if e.alive and Vector2(e.position.x - pos.x, e.position.z - pos.z).length() < radius + e.radius * 0.5:
			e.take_damage(dmg)
	var fx := MU.part(world, MU.sphere(1.0, 16, 8), MU.transparent(Color(1, 0.6, 0.15, 0.85), 5.0), pos + Vector3(0, 0.5, 0), Vector3.ZERO, Vector3.ONE * 0.3)
	var tw := fx.create_tween().set_parallel(true)
	tw.tween_property(fx, "scale", Vector3(radius, radius * 0.7, radius), 0.25)
	tw.tween_property(fx, "transparency", 1.0, 0.35)
	tw.chain().tween_callback(fx.queue_free)
	_shake = max(_shake, 0.35)


func _death_burst(pos: Vector3, s: float) -> void:
	if _death_fx_active >= MAX_DEATH_FX:
		return
	if _death_fx_mesh == null:
		_death_fx_mesh = MU.sphere(0.5, 8, 4)
		_death_fx_material = MU.transparent(Color(1, 0.35, 0.8, 0.7), 3.0)
	var fx := MU.part(world, _death_fx_mesh, _death_fx_material, pos + Vector3(0, 0.9 * s, 0), Vector3.ZERO, Vector3.ONE * 0.4 * s)
	_death_fx_active += 1
	var tw := fx.create_tween().set_parallel(true)
	tw.tween_property(fx, "scale", Vector3.ONE * 2.2 * s, 0.2)
	tw.tween_property(fx, "transparency", 1.0, 0.2)
	tw.chain().tween_callback(func():
		_death_fx_active -= 1
		fx.queue_free())


func _init_damage_labels() -> void:
	for i in DAMAGE_LABEL_POOL_SIZE:
		var label := Label3D.new()
		label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label.no_depth_test = true
		label.font_size = 36
		label.outline_size = 8
		label.pixel_size = 0.008
		label.modulate = Color(1, 0.9, 0.85)
		label.outline_modulate = Color(0.5, 0.05, 0.05)
		label.visible = false
		world.add_child(label)
		_damage_labels.append(label)
		_damage_label_ages.append(-1.0)


func _update_damage_labels(delta: float) -> void:
	for i in _damage_labels.size():
		if _damage_label_ages[i] < 0.0:
			continue
		_damage_label_ages[i] += delta
		var age := _damage_label_ages[i]
		var label := _damage_labels[i]
		label.position.y += delta * 2.0
		var color := label.modulate
		color.a = 1.0 - clampf(age / 0.4, 0.0, 1.0)
		label.modulate = color
		if age >= 0.4:
			label.visible = false
			_damage_label_ages[i] = -1.0


func damage_number(pos: Vector3, amount: float) -> void:
	if _dmg_label_timer > 0.0:
		return
	for i in _damage_labels.size():
		if _damage_label_ages[i] >= 0.0:
			continue
		var label := _damage_labels[i]
		label.text = str(int(round(amount)))
		label.position = pos + Vector3(randf_range(-0.3, 0.3), 0, randf_range(-0.3, 0.3))
		label.modulate = Color(1, 0.9, 0.85)
		label.visible = true
		_damage_label_ages[i] = 0.0
		_dmg_label_timer = 0.12
		return


# ---------------------------------------------------------------- consultas espaciales
func _rebuild_grid() -> void:
	_grid.clear()
	for e in enemies:
		if not is_instance_valid(e) or not e.alive:
			continue
		var k := Vector2i(floori(e.position.x / CELL), floori(e.position.z / CELL))
		if _grid.has(k):
			_grid[k].append(e)
		else:
			_grid[k] = [e]


func query(pos: Vector3, r: float) -> Array:
	var res := []
	var x0 := floori((pos.x - r) / CELL)
	var x1 := floori((pos.x + r) / CELL)
	var z0 := floori((pos.z - r) / CELL)
	var z1 := floori((pos.z + r) / CELL)
	for x in range(x0, x1 + 1):
		for z in range(z0, z1 + 1):
			var k := Vector2i(x, z)
			if _grid.has(k):
				for e in _grid[k]:
					if is_instance_valid(e) and e.alive:
						res.append(e)
	return res


func nearest_enemy(pos: Vector3, max_dist: float) -> Node3D:
	var best: Node3D = null
	var best_d := max_dist * max_dist
	for e in enemies:
		if not is_instance_valid(e) or not e.alive:
			continue
		var d: float = Vector2(e.position.x - pos.x, e.position.z - pos.z).length_squared()
		if d < best_d:
			best_d = d
			best = e
	return best


func random_enemy_near(pos: Vector3, max_dist: float) -> Node3D:
	var near := []
	for e in enemies:
		if is_instance_valid(e) and e.alive and e.position.distance_to(pos) < max_dist:
			near.append(e)
	return near.pick_random() if not near.is_empty() else null
