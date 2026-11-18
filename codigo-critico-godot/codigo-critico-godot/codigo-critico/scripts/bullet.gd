extends Node3D

const MU = preload("res://scripts/mesh_util.gd")
static var _mesh: Mesh
static var _mat: Material

var main: Node
var velocity := Vector3.ZERO
var damage := 10.0
var pierce_left := 0
var life := 1.1
var _hit: Array = []


func _ready() -> void:
	if _mesh == null:
		_mesh = MU.box(Vector3(0.09, 0.09, 0.6))
		_mat = MU.glow(Color(1.0, 0.75, 0.25), 4.0)
	MU.part(self, _mesh, _mat)
	look_at(position + velocity, Vector3.UP)


func _process(delta: float) -> void:
	position += velocity * delta
	life -= delta
	if life <= 0.0:
		queue_free()
		return
	for e in main.query(position, 2.0):
		if not e.alive or _hit.has(e):
			continue
		if Vector2(e.position.x - position.x, e.position.z - position.z).length() < e.radius + 0.2:
			_hit.append(e)
			e.take_damage(damage)
			pierce_left -= 1
			if pierce_left < 0:
				queue_free()
				return
