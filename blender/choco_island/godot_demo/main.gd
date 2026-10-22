## Choco Island demo: generates an island from a seed and orbits a camera round it.
##   left-drag orbit · right-drag / WASD pan · wheel zoom · Space auto-spin
##   N next seed · R random seed · 1 / 2 / 3 / 4 camera: overview / left-back / right-back / waterfall
##   5 river close-up · [ ] step through the chocolate river shader (river_steps/) · 0 finished shader
##
## Screenshots without touching anything:
##   Godot --path . -- --shot 1 2 3     → shot_<seed>_<view>.png for each seed, then quits
##   Godot --path . -- --river 1        → river_<step>.png, one per river shader step (seed 1), then quits
extends Node3D

const Island := preload("res://choco_island.gd")
const Palette := preload("res://choco_palette.gd")
const RIVER_STEPS := [
	"step01_colour", "step02_flow", "step03_streaks", "step04_banks", "step05_marbling",
	"step06_bubbles", "step07_toon_gloss", "step08_ripples", "step09_wobble",
]

const ORBIT_SPEED := 0.005
const ZOOM_STEP := 1.12
const PAN_SPEED := 10.0
## Camera views: [yaw, pitch, distance, focus]
const VIEWS := [
	[0.25, -0.38, 62.0, Vector3(0, 3, -1)],
	[-2.1, -0.3, 34.0, Vector3(0, 6, -7)],
	[1.9, -0.3, 34.0, Vector3(0, 6, -7)],
	[0.0, -0.15, 30.0, Vector3(1, 4, -2)],
]

var seed_value := 1
var yaw := 0.0
var pitch := 0.0
var distance := 0.0
var focus := Vector3.ZERO
var auto_spin := false

var island := Island.new()
var camera := Camera3D.new()
var label := Label.new()
var notes := Label.new()
var river_step := 0  # 0 = the finished choco_river.gdshader, 1–9 = river_steps/
var _shots: Array[int] = []
var _shot_view := 0
var _river_shots := false
var _wait := 0


func _ready() -> void:
	add_child(island)
	_add_light_and_sky()
	camera.fov = 45.0
	add_child(camera)
	_add_help()
	var args := OS.get_cmdline_user_args()
	if "--shot" in args:
		for a in args.slice(args.find("--shot") + 1):
			_shots.append(int(a))
		if _shots.is_empty():
			_shots.append(1)
		seed_value = _shots[0]
	elif "--river" in args:
		_river_shots = true
		var rest := args.slice(args.find("--river") + 1)
		seed_value = int(rest[0]) if not rest.is_empty() else 1
	_regenerate()
	_set_view(0)
	_set_river_step(1 if _river_shots else 0)
	if _river_shots:
		_river_view()


func _regenerate() -> void:
	var t0 := Time.get_ticks_msec()
	island.generate(seed_value)
	var ms := Time.get_ticks_msec() - t0
	label.text = "seed %d · %d blocks · %d steps · built in %d ms\n" % [seed_value, island.mountain.blocks.size(), island.stairs.steps.size(), ms] \
		+ "drag orbit · right-drag/WASD pan · wheel zoom · Space spin · N next · R random · 1-4 views · 5 river · [ ] river shader steps · 0 finished"
	print(label.text.get_slice("\n", 0))


## Close on the pool and the stream, looking upstream at the fall.
func _river_view() -> void:
	var pond := island.mountain.pond
	focus = Vector3(pond.x + 1.0, 1.0, island.mountain.lip_z() + 4.0)
	yaw = 0.55
	pitch = -0.42
	distance = 17.0
	_place_camera()


## Put river shader step i on every syrup surface (0 = the finished shader) and show its notes.
func _set_river_step(i: int) -> void:
	river_step = wrapi(i, 0, RIVER_STEPS.size() + 1)
	var path := "res://choco_river.gdshader" if river_step == 0 else "res://river_steps/%s.gdshader" % RIVER_STEPS[river_step - 1]
	Palette.set_river_shader(load(path))
	var title := "river shader: finished (choco_river.gdshader)" if river_step == 0 else "river shader step %d / %d" % [river_step, RIVER_STEPS.size()]
	notes.text = title + "\n" + _top_comment(path)


## The comment block at the top of a shader file, without the // marks.
static func _top_comment(path: String) -> String:
	var out: PackedStringArray = []
	for line in FileAccess.get_file_as_string(path).split("\n"):
		if not line.begins_with("//"):
			break
		out.append(line.trim_prefix("//").strip_edges())
	return "\n".join(out)


func _set_view(i: int) -> void:
	yaw = VIEWS[i][0]
	pitch = VIEWS[i][1]
	distance = VIEWS[i][2]
	focus = VIEWS[i][3]
	_place_camera()


func _add_light_and_sky() -> void:
	var sun := DirectionalLight3D.new()
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 140.0
	sun.rotation_degrees = Vector3(-50, 35, 0)
	sun.light_color = Color(1.0, 0.95, 0.88)
	sun.light_energy = 1.1
	add_child(sun)
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.98, 0.78, 0.86)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(1.0, 0.9, 0.93)
	env.ambient_light_energy = 0.75
	env.fog_enabled = true
	env.fog_light_color = Color(0.98, 0.78, 0.86)
	env.fog_density = 0.0025
	var world := WorldEnvironment.new()
	world.environment = env
	add_child(world)


func _add_help() -> void:
	label.position = Vector2(16, 12)
	label.add_theme_color_override("font_color", Color(0.35, 0.12, 0.2))
	notes.position = Vector2(16, 64)
	notes.add_theme_color_override("font_color", Color(0.3, 0.1, 0.05))
	notes.add_theme_font_size_override("font_size", 14)
	notes.add_theme_color_override("font_outline_color", Color(1, 0.93, 0.96))
	notes.add_theme_constant_override("outline_size", 6)
	var layer := CanvasLayer.new()
	layer.add_child(label)
	layer.add_child(notes)
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
			distance = minf(distance * ZOOM_STEP, 120.0)
	elif event is InputEventKey and event.pressed:
		match event.keycode:
			KEY_SPACE:
				auto_spin = not auto_spin
			KEY_N:
				seed_value += 1
				_regenerate()
			KEY_R:
				seed_value = randi() % 100000
				_regenerate()
			KEY_1:
				_set_view(0)
			KEY_2:
				_set_view(1)
			KEY_3:
				_set_view(2)
			KEY_4:
				_set_view(3)
			KEY_5:
				_river_view()
			KEY_BRACKETLEFT:
				_set_river_step(river_step - 1)
			KEY_BRACKETRIGHT:
				_set_river_step(river_step + 1)
			KEY_0:
				_set_river_step(0)
	_place_camera()


func _pan(screen_delta: Vector2) -> void:
	var right := Vector3.RIGHT.rotated(Vector3.UP, yaw)
	var forward := Vector3.FORWARD.rotated(Vector3.UP, yaw)
	focus += right * screen_delta.x - forward * screen_delta.y


func _process(delta: float) -> void:
	if not _shots.is_empty():
		_take_shots()
		return
	if _river_shots:
		_take_river_shots()
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


## Shot mode: each seed, each view, wait a few frames for shadows, save, move on, quit at the end.
func _take_shots() -> void:
	_wait += 1
	if _wait < 12:
		return
	_wait = 0
	var path := "res://shot_%d_%d.png" % [seed_value, _shot_view]
	get_viewport().get_texture().get_image().save_png(path)
	print("saved ", path)
	_shot_view += 1
	if _shot_view == VIEWS.size():
		_shot_view = 0
		_shots.pop_front()
		if _shots.is_empty():
			get_tree().quit()
			return
		seed_value = _shots[0]
		_regenerate()
	_set_view(_shot_view)


## River shot mode: one picture per shader step, then the finished shader, then quit.
func _take_river_shots() -> void:
	_wait += 1
	if _wait < 20:
		return
	_wait = 0
	var path := "res://river_%02d.png" % river_step
	get_viewport().get_texture().get_image().save_png(path)
	print("saved ", path)
	if river_step == 0:
		get_tree().quit()
		return
	_set_river_step(river_step + 1)
