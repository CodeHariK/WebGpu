extends Node3D
## The liquid shaders (water / lava / poison, mobile and desktop, and the slime) on two samples:
##   MC sample    the minecraft game's MC water: sample/water.res (the water mesh with the data the
##                shaders read: COLOR flow, UV, UV2, CUSTOM0 depth) and sample/terrain.res (the
##                ground round it), from the game's "last" inspect map (`make liquid-sample`)
##   slime river  sample/slime_river.glb, made in Blender (`make slime-river`,
##                tools/build_slime_river.py): a winding channel into a round pool between stone
##                banks, its surface dense enough for the slime's bulge and with the same COLOR
##                flow and UV (no falls, no depth)
## Lit like the game: sky, filmic tonemap, glow (the desktop lava's bloom needs it). N switches to
## night: a dim blue moon instead of the sun and a dark sky, so only what the shaders light
## themselves (EMISSION: lava, the poison's glow) stays bright.
##
## Keys: 1 - 7 or left / right pick the shader, M the sample, N day / night, drag to orbit, wheel to
## zoom, R the start view, F the whole map. `-- --capture-liquids` saves shots/liquid_<shader>.png,
## liquid_<shader>_night.png (MC sample) and liquid_<shader>_river(_night).png for each and quits
## (`--only=<shader file name>`: just that one).

const SHADERS := [
	["water, mobile", "res://water_mobile/water_mobile.gdshader"],
	["lava, mobile", "res://lava_mobile/lava_mobile.gdshader"],
	["green poison, mobile", "res://poison_mobile/poison_mobile.gdshader"],
	["water, desktop", "res://water_desktop/water_desktop.gdshader"],
	["lava, desktop", "res://lava_desktop/lava_desktop.gdshader"],
	["green poison, desktop", "res://poison_desktop/poison_desktop.gdshader"],
	["slime", "res://slime/slime.tres"], # a material: the shader and its baked textures
]
# Each sample: its name, the start view (looking at, distance, yaw, pitch in degrees) and the
# whole-map view (F).
const SAMPLES := [
	{"name": "MC sample", "at": Vector3(15.5, 1.5, 19.0), "distance": 6.5, "yaw": 45.0, "pitch": -29.5,
		"whole_at": Vector3(16, 0, 16), "whole_distance": 30.0}, # the pool, its stream and the fall above it
	{"name": "slime river", "at": Vector3(5.0, 0.0, -5.5), "distance": 6.5, "yaw": 150.0, "pitch": -42.0,
		"whole_at": Vector3(-2.5, 0, 0), "whole_distance": 20.0}, # where the river runs into the pool
]

var samples: Array[Node3D] = [] # each sample's root
var liquids: Array[MeshInstance3D] = [] # each sample's liquid surface
var sample := 0
var camera := Camera3D.new()
var sun := DirectionalLight3D.new() # the moon at night
var sky_material := ProceduralSkyMaterial.new()
var night := false
var label := Label.new()
var materials: Array[ShaderMaterial] = []
var current := 0
var at := Vector3.ZERO
var distance := 1.0
var yaw := 0.0
var pitch := 0.0
var dragging := false


func _ready() -> void:
	_add_world()
	_add_mc_sample()
	_add_slime_river()
	for entry in SHADERS:
		var path: String = entry[1]
		var material: ShaderMaterial
		if path.ends_with(".tres"):
			material = load(path)
		else:
			material = ShaderMaterial.new()
			material.shader = load(path)
		materials.append(material)
	camera.fov = 70.0
	add_child(camera)
	label.position = Vector2(14, 12)
	label.add_theme_font_size_override("font_size", 18)
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", 6)
	var layer := CanvasLayer.new()
	layer.add_child(label)
	add_child(layer)
	_show_sample(0)
	_show(0)
	if "--capture-liquids" in OS.get_cmdline_user_args():
		_capture()


# The game's MC water and the ground round it.
func _add_mc_sample() -> void:
	var root := Node3D.new()
	var terrain := MeshInstance3D.new()
	terrain.mesh = load("res://sample/terrain.res")
	root.add_child(terrain)
	var water := MeshInstance3D.new()
	water.mesh = load("res://sample/water.res")
	water.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	root.add_child(water)
	add_child(root)
	samples.append(root)
	liquids.append(water)


# The Blender river: its "banks" keep their stone material, its "slime" surface gets the shaders.
func _add_slime_river() -> void:
	var root: Node3D = load("res://sample/slime_river.glb").instantiate()
	add_child(root)
	var surface: MeshInstance3D = root.find_child("slime", true, false)
	surface.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	samples.append(root)
	liquids.append(surface)


# Sun, sky and the game's look: filmic tonemap, glow. The ambient light comes from the sky, so a
# dark night sky dims everything that isn't lit directly.
func _add_world() -> void:
	sun.shadow_enabled = true
	add_child(sun)
	var sky := Sky.new()
	sky.sky_material = sky_material
	var env := Environment.new()
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.glow_enabled = true
	var world := WorldEnvironment.new()
	world.environment = env
	add_child(world)
	_set_night(false)


# Day: the white sun high up and the default blue sky. Night: a dim blue moon (the sky draws its
# disc where the light comes from) and a near-black sky.
func _set_night(p_night: bool) -> void:
	night = p_night
	var fresh := ProceduralSkyMaterial.new() # the default (day) colours
	if night:
		sun.rotation_degrees = Vector3(-40, -130, 0)
		sun.light_color = Color(0.6, 0.7, 1.0)
		sun.light_energy = 0.25
		sky_material.sky_top_color = Color(0.004, 0.008, 0.03)
		sky_material.sky_horizon_color = Color(0.03, 0.04, 0.09)
		sky_material.ground_horizon_color = Color(0.03, 0.04, 0.08)
		sky_material.ground_bottom_color = Color(0.0, 0.0, 0.01)
		sky_material.sun_angle_max = 2.0 # a small moon
	else:
		sun.rotation_degrees = Vector3(-60, 150, 0)
		sun.light_color = Color.WHITE
		sun.light_energy = 1.0
		sky_material.sky_top_color = fresh.sky_top_color
		sky_material.sky_horizon_color = fresh.sky_horizon_color
		sky_material.ground_horizon_color = fresh.ground_horizon_color
		sky_material.ground_bottom_color = fresh.ground_bottom_color
		sky_material.sun_angle_max = fresh.sun_angle_max
	_update_label()


func _show(p_index: int) -> void:
	current = posmod(p_index, SHADERS.size())
	for liquid in liquids:
		liquid.material_override = materials[current]
	_update_label()


# Shows one sample (hiding the other) and puts the camera at its start view.
func _show_sample(p_index: int) -> void:
	sample = posmod(p_index, SAMPLES.size())
	for i in samples.size():
		samples[i].visible = i == sample
	_reset_view()
	_update_label()


func _reset_view() -> void:
	var s: Dictionary = SAMPLES[sample]
	at = s["at"]
	distance = s["distance"]
	yaw = deg_to_rad(s["yaw"])
	pitch = deg_to_rad(s["pitch"])
	_place_camera()


func _update_label() -> void:
	label.text = "%d / %d  %s  (%s, %s)\n%s\n1 - 7 or left / right: shader   M: sample   N: day / night   drag: orbit   wheel: zoom   R: start view   F: whole map" % [
			current + 1, SHADERS.size(), SHADERS[current][0], SAMPLES[sample]["name"], "night" if night else "day",
			SHADERS[current][1]]


func _place_camera() -> void:
	var offset := Vector3(cos(pitch) * sin(yaw), -sin(pitch), cos(pitch) * cos(yaw)) * distance
	camera.position = at + offset
	camera.look_at(at)


func _unhandled_input(p_event: InputEvent) -> void:
	if p_event is InputEventKey and p_event.pressed and not p_event.echo:
		var key: int = p_event.keycode
		if key >= KEY_1 and key < KEY_1 + SHADERS.size():
			_show(key - KEY_1)
		elif key == KEY_RIGHT:
			_show(current + 1)
		elif key == KEY_LEFT:
			_show(current - 1)
		elif key == KEY_N:
			_set_night(not night)
		elif key == KEY_M:
			_show_sample(sample + 1)
		elif key == KEY_R:
			_reset_view()
		elif key == KEY_F:
			at = SAMPLES[sample]["whole_at"]
			distance = SAMPLES[sample]["whole_distance"]
	elif p_event is InputEventMouseButton:
		if p_event.button_index == MOUSE_BUTTON_LEFT:
			dragging = p_event.pressed
		elif p_event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = max(distance * 0.9, 1.0)
		elif p_event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = min(distance * 1.1, 60.0)
	elif p_event is InputEventMouseMotion and dragging:
		yaw -= p_event.relative.x * 0.008
		pitch = clamp(pitch - p_event.relative.y * 0.008, deg_to_rad(-85.0), deg_to_rad(-5.0))
	_place_camera()


# One shot of each shader on each sample from its start view, by day and by night, then quit.
# `-- --capture-liquids --only=slime` shoots just the shaders whose file is named so.
func _capture() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	var ignore := FileAccess.open("res://shots/.gdignore", FileAccess.WRITE) # Godot: don't import the shots
	ignore.close()
	var only := ""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--only="):
			only = arg.trim_prefix("--only=")
	for s in SAMPLES.size():
		_show_sample(s)
		for at_night in [false, true]:
			_set_night(at_night)
			for i in SHADERS.size():
				if only != "" and String(SHADERS[i][1]).get_file().get_basename() != only:
					continue
				_show(i)
				for f in 20:
					await get_tree().process_frame
				await RenderingServer.frame_post_draw
				var file: String = "liquid_%s%s%s.png" % [String(SHADERS[i][1]).get_file().get_basename(),
						"_river" if s == 1 else "", "_night" if at_night else ""]
				get_viewport().get_texture().get_image().save_png(ProjectSettings.globalize_path("res://shots/" + file))
				print("saved shots/" + file)
	get_tree().quit()
