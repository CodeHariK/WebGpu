## Toxic Factory demo: the polluted factory by the river, trucks coming and going, smoke and smog.
##   left-drag orbit · right-drag / WASD pan · wheel zoom · Space auto-spin
##   1 overview · 2 outfalls and river · 3 gate and trucks · 4 chimneys · 5 from behind · 6 across the river
##
## Screenshots without touching anything:
##   Godot --path . -- --shot      → shot_<view>.png for every view (after a few seconds of traffic), then quits
extends Node3D

const Scene := preload("res://toxic_scene.gd")
const Traffic := preload("res://toxic_traffic.gd")
const Effects := preload("res://toxic_effects.gd")

const ORBIT_SPEED := 0.005
const ZOOM_STEP := 1.12
const PAN_SPEED := 12.0
## Camera views: [yaw, pitch, distance, focus]
const VIEWS := [
	[0.45, -0.42, 85.0, Vector3(0, 4, -6)],
	[0.35, -0.3, 26.0, Vector3(0, 0, 15)],
	[-0.5, -0.32, 34.0, Vector3(-8, 2, 2)],
	[0.9, -0.12, 55.0, Vector3(-4, 12, -20)],
	[3.0, -0.25, 70.0, Vector3(0, 4, -6)],
	[0.12, -0.12, 62.0, Vector3(0, 5, -4)],
]

var yaw := 0.0
var pitch := 0.0
var distance := 0.0
var focus := Vector3.ZERO
var auto_spin := false

var scene := Scene.new()
var traffic := Traffic.new()
var effects := Effects.new()
var camera := Camera3D.new()
var label := Label.new()
var _shots := false
var _shot_view := 0
var _wait := 0


func _ready() -> void:
	add_child(scene)
	var t0 := Time.get_ticks_msec()
	scene.build(1)
	add_child(traffic)
	traffic.build(scene.kit, scene.gates)
	add_child(effects)
	effects.build(scene.kit, scene.chimney_tops, scene.tower_tops, scene.vent_tops)
	print("built in %d ms" % (Time.get_ticks_msec() - t0))
	_add_light_and_sky()
	camera.fov = 45.0
	camera.far = 400.0
	add_child(camera)
	_add_help()
	_shots = "--shot" in OS.get_cmdline_user_args()
	_set_view(0)


func _set_view(i: int) -> void:
	yaw = VIEWS[i][0]
	pitch = VIEWS[i][1]
	distance = VIEWS[i][2]
	focus = VIEWS[i][3]
	_place_camera()


## Dirty yellow sky, a warm hazy sun, thick smog fog, and glow so the sludge shines.
func _add_light_and_sky() -> void:
	var sun := DirectionalLight3D.new()
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 160.0
	sun.rotation_degrees = Vector3(-48, 30, 0)
	sun.light_color = Color(1.0, 0.9, 0.72)
	sun.light_energy = 1.05
	add_child(sun)
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.8, 0.76, 0.58)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.92, 0.88, 0.74)
	env.ambient_light_energy = 0.7
	env.fog_enabled = true
	env.fog_light_color = Color(0.74, 0.68, 0.5)
	env.fog_density = 0.007
	env.glow_enabled = true
	env.glow_intensity = 0.6
	env.glow_bloom = 0.05
	var world := WorldEnvironment.new()
	world.environment = env
	add_child(world)


func _add_help() -> void:
	label.position = Vector2(16, 12)
	label.add_theme_color_override("font_color", Color(0.2, 0.18, 0.1))
	label.add_theme_color_override("font_outline_color", Color(0.95, 0.92, 0.8))
	label.add_theme_constant_override("outline_size", 6)
	label.text = "Toxic Factory · drag orbit · right-drag/WASD pan · wheel zoom · Space spin\n" \
		+ "1 overview · 2 outfalls · 3 gate + trucks · 4 chimneys · 5 from behind · 6 across the river"
	var layer := CanvasLayer.new()
	layer.add_child(label)
	add_child(layer)


func _place_camera() -> void:
	var offset := Vector3(0, 0, distance).rotated(Vector3.RIGHT, pitch).rotated(Vector3.UP, yaw)
	camera.position = focus + offset
	camera.look_at(focus)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		if event.button_mask & MOUSE_BUTTON_MASK_LEFT:
			yaw -= event.relative.x * ORBIT_SPEED
			pitch = clampf(pitch - event.relative.y * ORBIT_SPEED, deg_to_rad(-85), deg_to_rad(-3))
			auto_spin = false
		elif event.button_mask & MOUSE_BUTTON_MASK_RIGHT:
			_pan(Vector2(-event.relative.x, -event.relative.y) * distance * 0.0015)
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = maxf(distance / ZOOM_STEP, 4.0)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = minf(distance * ZOOM_STEP, 200.0)
	elif event is InputEventKey and event.pressed:
		if event.keycode == KEY_SPACE:
			auto_spin = not auto_spin
		elif event.keycode >= KEY_1 and event.keycode < KEY_1 + VIEWS.size():
			_set_view(event.keycode - KEY_1)
	_place_camera()


func _pan(screen_delta: Vector2) -> void:
	var right := Vector3.RIGHT.rotated(Vector3.UP, yaw)
	var forward := Vector3.FORWARD.rotated(Vector3.UP, yaw)
	focus += right * screen_delta.x - forward * screen_delta.y


func _process(delta: float) -> void:
	if _shots:
		_take_shots()
		return
	var move := Vector2.ZERO
	for key in [[KEY_D, Vector2.RIGHT], [KEY_A, Vector2.LEFT], [KEY_S, Vector2.DOWN], [KEY_W, Vector2.UP]]:
		if Input.is_key_pressed(key[0]):
			move += key[1]
	if move != Vector2.ZERO:
		_pan(move * PAN_SPEED * delta)
	if auto_spin:
		yaw += delta * 0.08
	_place_camera()


## Shot mode: let the trucks drive for a few seconds, then save each view and quit.
func _take_shots() -> void:
	_wait += 1
	if _wait < (300 if _shot_view == 0 else 15):
		return
	_wait = 0
	var path := "res://shot_%d.png" % _shot_view
	get_viewport().get_texture().get_image().save_png(path)
	print("saved ", path)
	_shot_view += 1
	if _shot_view == VIEWS.size():
		get_tree().quit()
		return
	_set_view(_shot_view)
