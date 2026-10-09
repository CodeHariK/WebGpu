## Sand Island demo: loads the island, puts the stylized sand/water shaders on it, and adds a sun,
## sky and an orbit camera.
## Controls: left-drag orbit · right-drag or WASD pan · wheel zoom · Space toggle auto-spin · R reset
extends Node3D

const ORBIT_SPEED := 0.005
const ZOOM_STEP := 1.12
const PAN_SPEED := 8.0

var yaw := deg_to_rad(-12.0)
var pitch := deg_to_rad(-30.0)
var distance := 22.0
var focus := Vector3(0.5, 0.0, 2.0)
var auto_spin := true

@onready var camera := Camera3D.new()


func _ready() -> void:
	_load_island()
	_add_light_and_sky()
	camera.fov = 40.0
	add_child(camera)
	_add_help()
	_place_camera()


func _load_island() -> void:
	var island: Node3D = load("res://sand_island.glb").instantiate()
	add_child(island)
	var sand := ShaderMaterial.new()
	sand.shader = load("res://sand.gdshader")
	var sea := ShaderMaterial.new()
	sea.shader = load("res://water.gdshader")
	for mesh in island.find_children("*", "MeshInstance3D", true, false):
		if mesh.name.begins_with("Terrain"):
			mesh.material_override = sand
		elif mesh.name.begins_with("Water"):
			mesh.material_override = sea


func _add_light_and_sky() -> void:
	var sun := DirectionalLight3D.new()
	sun.shadow_enabled = true
	sun.rotation_degrees = Vector3(-55, 30, 0)
	sun.light_color = Color(1.0, 0.97, 0.9)
	add_child(sun)
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.62, 0.82, 0.98)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.8, 0.85, 1.0)
	env.ambient_light_energy = 0.55
	var world := WorldEnvironment.new()
	world.environment = env
	add_child(world)


func _add_help() -> void:
	var label := Label.new()
	label.text = "left-drag orbit · right-drag / WASD pan · wheel zoom · Space spin · R reset"
	label.position = Vector2(16, 12)
	label.add_theme_color_override("font_color", Color(0.1, 0.25, 0.45))
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
			pitch = clampf(pitch - event.relative.y * ORBIT_SPEED, deg_to_rad(-85), deg_to_rad(-5))
			auto_spin = false
		elif event.button_mask & MOUSE_BUTTON_MASK_RIGHT:
			_pan(Vector2(-event.relative.x, -event.relative.y) * distance * 0.0015)
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = maxf(distance / ZOOM_STEP, 3.0)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = minf(distance * ZOOM_STEP, 60.0)
	elif event is InputEventKey and event.pressed:
		if event.keycode == KEY_SPACE:
			auto_spin = not auto_spin
		elif event.keycode == KEY_R:
			yaw = deg_to_rad(-12.0); pitch = deg_to_rad(-30.0); distance = 22.0; focus = Vector3(0.5, 0, 2)
	_place_camera()


func _pan(screen_delta: Vector2) -> void:
	var right := Vector3.RIGHT.rotated(Vector3.UP, yaw)
	var forward := Vector3.FORWARD.rotated(Vector3.UP, yaw)
	focus += right * screen_delta.x - forward * screen_delta.y


func _process(delta: float) -> void:
	var move := Vector2(
		Input.get_action_strength("ui_right") - Input.get_action_strength("ui_left"),
		Input.get_action_strength("ui_down") - Input.get_action_strength("ui_up"))
	for key in [[KEY_D, Vector2.RIGHT], [KEY_A, Vector2.LEFT], [KEY_S, Vector2.DOWN], [KEY_W, Vector2.UP]]:
		if Input.is_key_pressed(key[0]):
			move += key[1]
	if move != Vector2.ZERO:
		_pan(move * PAN_SPEED * delta)
	if auto_spin:
		yaw += delta * 0.08
	_place_camera()
