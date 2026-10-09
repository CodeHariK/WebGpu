## Snow island: the final snow shader (snow.gdshader) on a lofted procedural island, icy water
## (water step 11 with cold colours) and an optional snow-cap sheet over the plateau.
## Keys: V bank style · L snow cap on/off · N new island · F wireframe · H hide perf · R reset view
##       left-drag orbit · Shift/right-drag pan · wheel zoom
## Run with `-- --capture-snow` to save screenshots into shots/snow/ and quit.
extends Node3D

var noise_texture: Texture2D = load("res://textures/noise_128.png")
var snow_material := ShaderMaterial.new()
var cap_material := ShaderMaterial.new()
var water_material := ShaderMaterial.new()
var island: LoftIsland
var island_seed := 1
var bank_style: BankProfile.Style = BankProfile.Style.ROUND
var snow_cap := true
var hud := PerfHud.new()
var camera := Camera3D.new()
var status := Label.new()
var yaw := deg_to_rad(-20.0)
var pitch := deg_to_rad(-30.0)
var distance := 20.0
var focus := Vector3(0.0, 0.6, 0.0)


func _ready() -> void:
	RenderingServer.set_debug_generate_wireframes(true)  # must be on before meshes are created
	_make_materials()
	island = LoftIsland.new(snow_material, water_material)
	island.layer_material = cap_material
	add_child(island)
	_rebuild()
	_add_world()
	camera.fov = 40.0
	add_child(camera)
	_add_panel()
	add_child(hud)
	_place_camera()
	if "--capture-snow" in OS.get_cmdline_user_args():
		_capture()


func _make_materials() -> void:
	snow_material.shader = load("res://snow/snow.gdshader")
	snow_material.set_shader_parameter("noise_tex", noise_texture)
	cap_material.shader = snow_material.shader
	cap_material.set_shader_parameter("noise_tex", noise_texture)
	cap_material.set_shader_parameter("snow_everywhere", 1.0)
	water_material.shader = load("res://steps/step11_noise_texture.gdshader")
	water_material.set_shader_parameter("noise_tex", noise_texture)
	water_material.set_shader_parameter("shore_colour", Color(0.78, 0.93, 0.96))
	water_material.set_shader_parameter("shallow_colour", Color(0.42, 0.7, 0.86))
	water_material.set_shader_parameter("deep_colour", Color(0.14, 0.28, 0.6))


## A puffy snow sheet over the plateau: wider than the body, rounded edge, drippy bottom.
func _cap_layers() -> Array[IslandLayer]:
	return IslandLayer.preset(3 if snow_cap else 0, island.loft.plateau_height)


func _rebuild() -> void:
	island.regenerate(island_seed, bank_style)
	island.layers = _cap_layers()
	island.rebuild_layers()
	_update_status()


func _add_world() -> void:
	var sun := DirectionalLight3D.new()
	sun.shadow_enabled = true
	sun.light_color = Color(1.0, 0.97, 0.92)
	sun.rotation_degrees = Vector3(-34, 95, 0)   # low winter sun from the side: long blue shadows
	add_child(sun)
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.7, 0.82, 0.95)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.75, 0.82, 1.0)
	env.ambient_light_energy = 0.5
	var world := WorldEnvironment.new()
	world.environment = env
	add_child(world)


func _add_panel() -> void:
	var layer := CanvasLayer.new()
	add_child(layer)
	var box := VBoxContainer.new()
	box.position = Vector2(16, 12)
	layer.add_child(box)
	var title := Label.new()
	title.text = "Snow island — snow/snow.gdshader"
	title.add_theme_font_size_override("font_size", 22)
	box.add_child(title)
	var keys := Label.new()
	keys.text = "V bank style · L snow cap · N new island · F wireframe · H perf · R reset · drag orbit · Shift-drag pan · wheel zoom"
	keys.add_theme_font_size_override("font_size", 14)
	box.add_child(keys)
	box.add_child(status)
	for label in [title, keys, status]:
		label.add_theme_color_override("font_color", Color(0.12, 0.16, 0.28))


func _update_status() -> void:
	status.text = "island %d · bank: %s · snow cap: %s" % [island_seed, BankProfile.NAMES[bank_style], "on" if snow_cap else "off"]


func _place_camera() -> void:
	camera.position = focus + Vector3(0, 0, distance).rotated(Vector3.RIGHT, pitch).rotated(Vector3.UP, yaw)
	camera.look_at(focus)


func _pan(mouse_delta: Vector2) -> void:
	var basis := camera.global_transform.basis
	focus += (-basis.x * mouse_delta.x + basis.y * mouse_delta.y) * distance * 0.0015


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed:
		match event.keycode:
			KEY_V:
				bank_style = (bank_style + 1) % BankProfile.NAMES.size() as BankProfile.Style
				_rebuild()
			KEY_L:
				snow_cap = not snow_cap
				island.layers = _cap_layers()
				island.rebuild_layers()
				_update_status()
			KEY_N:
				island_seed += 1
				_rebuild()
			KEY_F:
				var vp := get_viewport()
				vp.debug_draw = Viewport.DEBUG_DRAW_DISABLED if vp.debug_draw == Viewport.DEBUG_DRAW_WIREFRAME else Viewport.DEBUG_DRAW_WIREFRAME
			KEY_H:
				hud.visible = not hud.visible
			KEY_P:
				hud.measure_now()
			KEY_R:
				yaw = deg_to_rad(-20.0); pitch = deg_to_rad(-30.0); distance = 20.0; focus = Vector3(0.0, 0.6, 0.0)
	elif event is InputEventMouseMotion and event.button_mask & MOUSE_BUTTON_MASK_LEFT and event.shift_pressed:
		_pan(event.relative)
	elif event is InputEventMouseMotion and event.button_mask & (MOUSE_BUTTON_MASK_RIGHT | MOUSE_BUTTON_MASK_MIDDLE):
		_pan(event.relative)
	elif event is InputEventMouseMotion and event.button_mask & MOUSE_BUTTON_MASK_LEFT:
		yaw -= event.relative.x * 0.005
		pitch = clampf(pitch - event.relative.y * 0.005, deg_to_rad(-85), deg_to_rad(-5))
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = maxf(distance / 1.12, 3.0)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = minf(distance * 1.12, 60.0)
	_place_camera()


## Screenshots: the four bank styles (cap on), no cap, and a close-up of the snow surface.
func _capture() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots/snow"))
	var shots := [
		[BankProfile.Style.WALL, true, 20.0], [BankProfile.Style.SLOPE, true, 20.0],
		[BankProfile.Style.ROUND, true, 20.0], [BankProfile.Style.OVERHANG, true, 20.0],
		[BankProfile.Style.ROUND, false, 20.0], [BankProfile.Style.ROUND, true, 7.0],
	]
	for i in shots.size():
		bank_style = shots[i][0]
		snow_cap = shots[i][1]
		distance = shots[i][2]
		_rebuild()
		_place_camera()
		for f in 4:
			await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("res://shots/snow/snow_%d.png" % (i + 1))
	print("snow capture done")
	get_tree().quit()
