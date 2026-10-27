## Step 1 — asset viewer. The cultist, the first-person arms with the revolver, and the small props
## (flashlight, speed loader, bullet, shell), all wearing our mobile materials, with what each one
## costs (triangles, surfaces ≈ draw calls, bones) and live FPS / draw calls / primitives.
##
##   1 cultist · 2 arms in first person · 3 props · 4 everything      [ ] previous / next arms animation
##   L studio light ↔ dark with flashlight (how it will look in game)
##   left-drag orbit · right-drag pan · wheel zoom
##
## Screenshots: Godot --path . res://demos/step01_viewer.tscn -- --shot  → shots/step01_*.png
extends Node3D

const Look := preload("res://src/look/character_look.gd")

const CULTIST := "res://assets/enemy/Cultist.gltf"
const ARMS := "res://assets/player/ArmsAnimations.gltf"
const PROPS := [  # path, gun metal?
	["res://assets/player/FlashlightMesh.gltf", false],
	["res://assets/player/SpeedLoader.gltf", true],
	["res://assets/player/Bullet.gltf", false],
	["res://assets/player/EmptyShell.gltf", false],
]
## The arms are a first-person rig: like the original (PlayerBody.tscn) their origin sits 0.1 m
## above the camera, looking down -Z. View 2 looks through that camera.
const ARMS_AT := Vector3(3.5, 1.6, 0.0)
const FIRST_PERSON := 1
## Orbit views: [focus, distance, yaw, pitch] (view 2 is first person, the numbers are unused)
const VIEWS := [
	[Vector3(-1.6, 1.35, 0), 4.6, 0.35, -0.1],
	[Vector3(3.5, 1.5, 0), 1.0, 0.0, 0.0],
	[Vector3(0, 0.7, 1.4), 1.3, 0.0, -0.45],
	[Vector3(0.8, 1.2, 0.3), 8.0, 0.15, -0.2],
]

var focus := Vector3.ZERO
var distance := 4.0
var yaw := 0.0
var pitch := 0.0

var camera := Camera3D.new()
var label := Label.new()
var sun := DirectionalLight3D.new()
var torch := SpotLight3D.new()
var env := Environment.new()
var first_person := false
var anim_player: AnimationPlayer
var anim_names: PackedStringArray = []
var anim_index := 0
var dark := false
var costs := {}
var _shots := false
var _shot := 0
var _wait := 0


func _ready() -> void:
	_build_stage()
	var cultist := _spawn(CULTIST, Vector3(-1.6, 0, 0))
	Look.dress_cultist(cultist)
	costs["cultist"] = Look.stats(cultist)
	var arms: Node3D = load(ARMS).instantiate()
	add_child(arms)
	arms.position = ARMS_AT
	Look.dress_arms(arms)
	costs["arms"] = Look.stats(arms)
	_setup_animation(arms)
	var props_cost := {"tris": 0, "surfaces": 0, "bones": 0}
	for i in PROPS.size():
		var prop := _spawn(PROPS[i][0], Vector3(-0.45 + i * 0.3, 0.6, 1.4))
		if PROPS[i][0].contains("Flashlight"):
			Look.dress_flashlight(prop)
		else:
			Look.dress_ammo(prop, PROPS[i][1])
		var c := Look.stats(prop)
		for k in props_cost:
			props_cost[k] += c[k]
	costs["props"] = props_cost
	_shots = "--shot" in OS.get_cmdline_user_args()
	_set_view(3)


## Load a glTF scene and stand it on the floor at `at` (its lowest point at y = at.y).
func _spawn(path: String, at: Vector3) -> Node3D:
	var node: Node3D = load(path).instantiate()
	add_child(node)
	var box := _bounds(node)
	node.position = at - Vector3(box.get_center().x, box.position.y, box.get_center().z)
	return node


func _bounds(root: Node) -> AABB:
	var box := AABB()
	var first := true
	for mesh: MeshInstance3D in root.find_children("*", "MeshInstance3D", true, false):
		var b := mesh.global_transform * mesh.get_aabb()
		box = b if first else box.merge(b)
		first = false
	return box


func _setup_animation(arms: Node) -> void:
	var players := arms.find_children("*", "AnimationPlayer", true, false)
	if players.is_empty():
		return
	anim_player = players[0]
	anim_names = anim_player.get_animation_list()
	anim_player.animation_finished.connect(func(_n): anim_player.play(anim_names[anim_index]))
	_play(0)


func _play(i: int) -> void:
	if anim_player == null:
		return
	anim_index = wrapi(i, 0, anim_names.size())
	anim_player.play(anim_names[anim_index])


func _build_stage() -> void:
	var floor_mesh := MeshInstance3D.new()
	var plane := PlaneMesh.new()
	plane.size = Vector2(12, 12)
	floor_mesh.mesh = plane
	var floor_mat := StandardMaterial3D.new()
	floor_mat.albedo_color = Color(0.22, 0.22, 0.24)
	floor_mat.roughness = 0.9
	floor_mesh.material_override = floor_mat
	add_child(floor_mesh)
	var stand := MeshInstance3D.new()  # a little table for the props
	var box := BoxMesh.new()
	box.size = Vector3(1.4, 0.6, 0.5)
	stand.mesh = box
	stand.material_override = floor_mat
	stand.position = Vector3(0, 0.3, 1.4)
	add_child(stand)
	sun.shadow_enabled = true
	sun.rotation_degrees = Vector3(-45, 35, 0)
	add_child(sun)
	torch.spot_range = 12.0
	torch.spot_angle = 28.0
	torch.light_energy = 6.0
	torch.shadow_enabled = true
	torch.light_projector = load("res://assets/player/textures/Flashlight_Cookie.png")
	torch.light_color = Color(1.0, 0.85, 0.65)
	camera.add_child(torch)
	camera.fov = 50.0
	camera.near = 0.02
	add_child(camera)
	var sky := Sky.new()
	sky.sky_material = ProceduralSkyMaterial.new()  # metals need something to reflect
	env.sky = sky
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.glow_enabled = true
	var world := WorldEnvironment.new()
	world.environment = env
	add_child(world)
	label.position = Vector2(14, 10)
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", 5)
	var layer := CanvasLayer.new()
	layer.add_child(label)
	add_child(layer)
	_set_dark(false)


## Studio: sky, sun and sky light. Dark: near black, only the flashlight on the camera.
func _set_dark(on: bool) -> void:
	dark = on
	sun.visible = not on
	torch.visible = on
	env.background_mode = Environment.BG_COLOR if on else Environment.BG_SKY
	env.background_color = Color(0.01, 0.01, 0.015)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR if on else Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_color = Color(0.25, 0.28, 0.35)
	env.ambient_light_energy = 0.08 if on else 1.0
	env.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED if on else Environment.REFLECTION_SOURCE_SKY


func _set_view(i: int) -> void:
	first_person = i == FIRST_PERSON
	camera.fov = 60.0 if first_person else 50.0
	focus = VIEWS[i][0]
	distance = VIEWS[i][1]
	yaw = VIEWS[i][2]
	pitch = VIEWS[i][3]


func _place_camera() -> void:
	if first_person:  # through the player's eyes: drag looks around a little
		camera.transform = Transform3D(Basis.from_euler(Vector3(pitch, yaw, 0)), ARMS_AT - Vector3(0, 0.1, 0))
		return
	var offset := Vector3(0, 0, distance).rotated(Vector3.RIGHT, pitch).rotated(Vector3.UP, yaw)
	camera.position = focus + offset
	camera.look_at(focus)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		if event.button_mask & MOUSE_BUTTON_MASK_LEFT:
			yaw -= event.relative.x * 0.006
			pitch = clampf(pitch - event.relative.y * 0.006, -1.4, 0.6)
		elif event.button_mask & MOUSE_BUTTON_MASK_RIGHT:
			var right := Vector3.RIGHT.rotated(Vector3.UP, yaw)
			focus += (-right * event.relative.x + Vector3.UP * event.relative.y) * distance * 0.0015
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = maxf(distance / 1.1, 0.3)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = minf(distance * 1.1, 20.0)
	elif event is InputEventKey and event.pressed:
		match event.keycode:
			KEY_1, KEY_2, KEY_3, KEY_4:
				_set_view(event.keycode - KEY_1)
			KEY_BRACKETLEFT:
				_play(anim_index - 1)
			KEY_BRACKETRIGHT:
				_play(anim_index + 1)
			KEY_L:
				_set_dark(not dark)


func _process(_delta: float) -> void:
	_place_camera()
	label.text = _readout()
	if _shots:
		_take_shots()


func _readout() -> String:
	var lines: PackedStringArray = ["Step 1 · asset viewer   (1 cultist · 2 arms · 3 props · 4 all · [ ] animation · L light)"]
	for key in ["cultist", "arms", "props"]:
		var c: Dictionary = costs.get(key, {})
		if not c.is_empty():
			lines.append("%-8s %6d tris  %2d surfaces  %2d bones" % [key, c.tris, c.surfaces, c.bones])
	if anim_player:
		lines.append("arms animation: %s (%d/%d)" % [anim_names[anim_index], anim_index + 1, anim_names.size()])
	lines.append("frame: %d fps · %d draw calls · %d primitives" % [
		Engine.get_frames_per_second(),
		Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME),
		Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME)])
	return "\n".join(lines)


## Shot mode: every view in studio light, then the all view and the arms view in the dark.
func _take_shots() -> void:
	const PLAN := [[0, false], [1, false], [2, false], [3, false], [3, true], [1, true]]
	_wait += 1
	if _wait == 1:
		_set_view(PLAN[_shot][0])
		_set_dark(PLAN[_shot][1])
		_play(_shot % anim_names.size() if anim_player else 0)
	if _wait < (90 if _shot == 0 else 30):
		return
	_wait = 0
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	var path := "res://shots/step01_%d.png" % _shot
	get_viewport().get_texture().get_image().save_png(path)
	print("saved ", path, "  ", label.text.get_slice("\n", label.text.get_slice_count("\n") - 1))
	_shot += 1
	if _shot == PLAN.size():
		_shots = false
		get_tree().quit()
