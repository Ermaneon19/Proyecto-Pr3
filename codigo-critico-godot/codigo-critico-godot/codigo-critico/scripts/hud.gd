extends CanvasLayer
## Interfaz estilo Vampire Survivors: barra de XP arriba, nivel, cronómetro,
## bajas y núcleos, barra de vida bajo el jugador, panel de subida de nivel
## y base/tienda de mejoras permanentes (parte incremental).

signal deploy_pressed
signal upgrade_chosen(id: String)

var main: Node

var root: Control
var xp_bar: ProgressBar
var lvl_label: Label
var timer_label: Label
var kills_label: Label
var cores_label: Label
var weapons_label: Label
var hp_bar: ProgressBar
var toast_label: Label
var levelup_layer: Control
var levelup_box: VBoxContainer
var base_layer: Control
var base_content: VBoxContainer

const BLUE := Color(0.22, 0.45, 0.95)
const AMBER := Color(0.95, 0.65, 0.15)
const PANEL_BG := Color(0.06, 0.07, 0.09, 0.96)


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	root = Control.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.theme = _make_theme()
	add_child(root)
	_build_top_bar()
	_build_hp_bar()
	_build_levelup()
	_build_base()


# ---------------------------------------------------------------- tema
func _make_theme() -> Theme:
	var t := Theme.new()
	t.default_font_size = 18
	t.set_color("font_outline_color", "Label", Color.BLACK)
	t.set_constant("outline_size", "Label", 6)
	t.set_stylebox("normal", "Button", _box(Color(0.12, 0.14, 0.18), AMBER.darkened(0.5), 2))
	t.set_stylebox("hover", "Button", _box(Color(0.2, 0.18, 0.12), AMBER, 2))
	t.set_stylebox("pressed", "Button", _box(Color(0.3, 0.22, 0.08), AMBER, 2))
	t.set_stylebox("disabled", "Button", _box(Color(0.08, 0.08, 0.09), Color(0.25, 0.25, 0.25), 2))
	t.set_stylebox("focus", "Button", StyleBoxEmpty.new())
	t.set_stylebox("panel", "PanelContainer", _box(PANEL_BG, AMBER, 3, 18))
	return t


func _box(bg: Color, border: Color, bw := 0, pad := 10) -> StyleBoxFlat:
	var s := StyleBoxFlat.new()
	s.bg_color = bg
	s.border_color = border
	s.set_border_width_all(bw)
	s.set_corner_radius_all(4)
	s.set_content_margin_all(pad)
	return s


func _label(text: String, size := 18, color := Color.WHITE) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return l


# ---------------------------------------------------------------- HUD en partida
func _build_top_bar() -> void:
	xp_bar = ProgressBar.new()
	xp_bar.show_percentage = false
	xp_bar.set_anchors_preset(Control.PRESET_TOP_WIDE)
	xp_bar.offset_left = 4
	xp_bar.offset_right = -4
	xp_bar.offset_top = 4
	xp_bar.offset_bottom = 28
	var bg := _box(Color(0.02, 0.02, 0.03), Color(0.75, 0.6, 0.3), 2, 0)
	xp_bar.add_theme_stylebox_override("background", bg)
	xp_bar.add_theme_stylebox_override("fill", _box(BLUE, Color(0, 0, 0, 0), 0, 0))
	xp_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(xp_bar)

	lvl_label = _label("LV 1", 18)
	lvl_label.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	lvl_label.position = Vector2(-80, 4)
	lvl_label.size = Vector2(70, 24)
	lvl_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	root.add_child(lvl_label)

	timer_label = _label("00:00", 40)
	timer_label.set_anchors_preset(Control.PRESET_CENTER_TOP)
	timer_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	timer_label.position = Vector2(-100, 32)
	timer_label.size = Vector2(200, 50)
	root.add_child(timer_label)

	var right := VBoxContainer.new()
	right.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	right.position = Vector2(-230, 36)
	right.size = Vector2(220, 60)
	right.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(right)
	kills_label = _label("BAJAS  0", 20)
	kills_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	cores_label = _label("NÚCLEOS  0", 20, AMBER)
	cores_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	right.add_child(kills_label)
	right.add_child(cores_label)

	weapons_label = _label("", 16, Color(0.8, 0.9, 1.0))
	weapons_label.position = Vector2(10, 36)
	root.add_child(weapons_label)

	toast_label = _label("", 26, Color(1, 0.4, 0.3))
	toast_label.set_anchors_preset(Control.PRESET_CENTER_TOP)
	toast_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	toast_label.position = Vector2(-300, 90)
	toast_label.size = Vector2(600, 40)
	toast_label.modulate.a = 0.0
	root.add_child(toast_label)


func _build_hp_bar() -> void:
	hp_bar = ProgressBar.new()
	hp_bar.show_percentage = false
	hp_bar.size = Vector2(56, 8)
	hp_bar.custom_minimum_size = Vector2(56, 8)
	hp_bar.add_theme_stylebox_override("background", _box(Color(0.05, 0.05, 0.05), Color.BLACK, 1, 0))
	hp_bar.add_theme_stylebox_override("fill", _box(Color(0.85, 0.1, 0.1), Color(0, 0, 0, 0), 0, 0))
	hp_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(hp_bar)


func _process(_delta: float) -> void:
	if main == null or main.player == null:
		return
	var p = main.player
	xp_bar.max_value = main.xp_next
	xp_bar.value = main.xp
	lvl_label.text = "LV %d" % main.level
	var t := int(main.run_time)
	timer_label.text = "%02d:%02d" % [t / 60, t % 60]
	kills_label.text = "BAJAS  %d" % main.kills
	cores_label.text = "NÚCLEOS  %d" % int(main.run_cores)

	var w := "Rifle  Nv.%d" % p.rifle_level
	if p.strike_level > 0:
		w += "\nBombardeo orbital  Nv.%d" % p.strike_level
	if p.drone_level > 0:
		w += "\nDrones  Nv.%d" % p.drone_level
	weapons_label.text = w

	hp_bar.max_value = p.max_hp
	hp_bar.value = p.hp
	hp_bar.visible = main.state != "menu" and p.alive
	var cam: Camera3D = main.camera
	var screen := cam.unproject_position(p.global_position + Vector3(0, -0.3, 0.6))
	hp_bar.position = screen - Vector2(28, 0)

	var in_run: bool = main.state != "menu"
	xp_bar.visible = in_run
	lvl_label.visible = in_run
	timer_label.visible = in_run
	kills_label.visible = in_run
	cores_label.visible = in_run
	weapons_label.visible = in_run


func toast(text: String) -> void:
	toast_label.text = text
	var tw := toast_label.create_tween()
	tw.tween_property(toast_label, "modulate:a", 1.0, 0.2)
	tw.tween_interval(2.0)
	tw.tween_property(toast_label, "modulate:a", 0.0, 0.5)


# ---------------------------------------------------------------- subida de nivel
func _build_levelup() -> void:
	levelup_layer = Control.new()
	levelup_layer.set_anchors_preset(Control.PRESET_FULL_RECT)
	levelup_layer.visible = false
	root.add_child(levelup_layer)
	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.55)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	levelup_layer.add_child(dim)

	var center := CenterContainer.new()
	center.set_anchors_preset(Control.PRESET_FULL_RECT)
	levelup_layer.add_child(center)
	var panel := PanelContainer.new()
	center.add_child(panel)
	levelup_box = VBoxContainer.new()
	levelup_box.add_theme_constant_override("separation", 12)
	panel.add_child(levelup_box)


func show_levelup(options: Array) -> void:
	for c in levelup_box.get_children():
		c.queue_free()
	var title := _label("¡SUBISTE DE NIVEL!", 32, AMBER)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	levelup_box.add_child(title)
	for o in options:
		var lvl: int = main.level_of(o.id)
		var tag := "NUEVO" if lvl == 0 and (o.id == "strike" or o.id == "drones") else "Nv. %d" % (lvl + 1)
		var btn := _card_button(o.title, o.desc, tag)
		btn.pressed.connect(_pick.bind(o.id))
		levelup_box.add_child(btn)
	levelup_layer.visible = true


func _card_button(title: String, desc: String, tag: String) -> Button:
	var btn := Button.new()
	btn.custom_minimum_size = Vector2(460, 78)
	var m := MarginContainer.new()
	m.set_anchors_preset(Control.PRESET_FULL_RECT)
	m.mouse_filter = Control.MOUSE_FILTER_IGNORE
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 12)
	btn.add_child(m)
	var h := HBoxContainer.new()
	h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	m.add_child(h)
	var v := VBoxContainer.new()
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	h.add_child(v)
	v.add_child(_label(title, 22, Color.WHITE))
	v.add_child(_label(desc, 16, Color(0.75, 0.8, 0.85)))
	h.add_child(_label(tag, 18, AMBER))
	return btn


func _pick(id: String) -> void:
	levelup_layer.visible = false
	upgrade_chosen.emit(id)


# ---------------------------------------------------------------- base / tienda incremental
func _build_base() -> void:
	base_layer = Control.new()
	base_layer.set_anchors_preset(Control.PRESET_FULL_RECT)
	base_layer.visible = false
	root.add_child(base_layer)
	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.6)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	base_layer.add_child(dim)
	var center := CenterContainer.new()
	center.set_anchors_preset(Control.PRESET_FULL_RECT)
	base_layer.add_child(center)
	var panel := PanelContainer.new()
	panel.custom_minimum_size = Vector2(640, 0)
	center.add_child(panel)
	base_content = VBoxContainer.new()
	base_content.add_theme_constant_override("separation", 8)
	panel.add_child(base_content)


func show_base() -> void:
	_refresh_base()
	base_layer.visible = true


func hide_base() -> void:
	base_layer.visible = false


func _refresh_base() -> void:
	for c in base_content.get_children():
		c.queue_free()

	var dead: bool = main.state == "dead"
	var title := _label("MISIÓN FALLIDA" if dead else "CÓDIGO CRÍTICO", 38, Color(1, 0.35, 0.3) if dead else AMBER)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	base_content.add_child(title)

	if dead and not GameData.last_run.is_empty():
		var r: Dictionary = GameData.last_run
		var t := int(r.time)
		var s := _label("Sobreviviste %02d:%02d  ·  %d bajas  ·  +%d núcleos" % [t / 60, t % 60, r.kills, r.earned], 18)
		s.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		base_content.add_child(s)
	else:
		var s := _label("Sobrevive a la plaga de insectos mecánicos. Moverse: WASD · Disparo automático", 16, Color(0.75, 0.8, 0.85))
		s.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		base_content.add_child(s)

	var bt := int(GameData.best_time)
	var rec := _label("Récord: %02d:%02d  ·  %d bajas" % [bt / 60, bt % 60, GameData.best_kills], 15, Color(0.6, 0.65, 0.7))
	rec.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	base_content.add_child(rec)

	base_content.add_child(HSeparator.new())
	var wallet := _label("NÚCLEOS DISPONIBLES:  %d" % GameData.cores, 24, AMBER)
	wallet.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	base_content.add_child(wallet)
	base_content.add_child(_label("ARMERÍA (mejoras permanentes)", 16, Color(0.7, 0.75, 0.8)))

	for id in GameData.UPGRADES:
		var u: Dictionary = GameData.UPGRADES[id]
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", 10)
		var info := VBoxContainer.new()
		info.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		info.add_theme_constant_override("separation", 0)
		info.add_child(_label("%s   [%d/%d]" % [u.name, GameData.get_level(id), u.max], 18))
		info.add_child(_label(u.desc, 14, Color(0.65, 0.7, 0.75)))
		row.add_child(info)
		var b := Button.new()
		b.custom_minimum_size = Vector2(150, 40)
		if GameData.is_maxed(id):
			b.text = "MÁXIMO"
			b.disabled = true
		else:
			b.text = "%d núcleos" % GameData.get_cost(id)
			b.disabled = not GameData.can_buy(id)
		b.pressed.connect(_buy.bind(id))
		row.add_child(b)
		base_content.add_child(row)

	base_content.add_child(HSeparator.new())
	var deploy := Button.new()
	deploy.text = "DESPLEGAR"
	deploy.custom_minimum_size = Vector2(0, 56)
	deploy.add_theme_font_size_override("font_size", 26)
	deploy.pressed.connect(func(): deploy_pressed.emit())
	base_content.add_child(deploy)


func _buy(id: String) -> void:
	if GameData.buy(id):
		_refresh_base()
