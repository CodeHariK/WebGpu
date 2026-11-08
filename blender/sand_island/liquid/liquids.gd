extends Node3D
## The six liquid shaders (water / lava / poison, mobile and desktop) on a sample of the minecraft
## game's MC water: sample/water.res (the water mesh with the data the shaders read: COLOR flow,
## UV, UV2, CUSTOM0 depth) and sample/terrain.res (the ground round it). Both come from the game's
## "last" inspect map: `make liquid-sample` (../Makefile, tools/export_liquid_sample.gd) exports
## them again.
## Lit like the game: sky, filmic tonemap, glow (the desktop lava's bloom needs it). N switches to
## night: a dim blue moon instead of the sun and a dark sky, so only what the shaders light
## themselves (EMISSION: lava, the poison's glow) stays bright.
##
## Keys: 1 - 6 or left / right pick the shader, N day / night, drag to orbit, wheel to zoom, R the
## start view, F the whole map. `-- --capture-liquids` saves shots/liquid_<shader>.png and
## liquid_<shader>_night.png for each and quits.

const SHADERS := [
	["water, mobile", "res://water_mobile/water_mobile.gdshader"],
	["lava, mobile", "res://lava_mobile/lava_mobile.gdshader"],
	["green poison, mobile", "res://poison_mobile/poison_mobile.gdshader"],
	["water, desktop", "res://water_desktop/water_desktop.gdshader"],
	["lava, desktop", "res://lava_desktop/lava_desktop.gdshader"],
	["green poison, desktop", "res://poison_desktop/poison_desktop.gdshader"],
]
const START_AT := Vector3(15.5, 1.5, 19.0) # the pool, its stream and the fall above it
const START_DISTANCE := 6.5

var water := MeshInstance3D.new()
var camera := Camera3D.new()
var sun := DirectionalLight3D.new() # the moon at night
var sky_material := ProceduralSkyMaterial.new()
var night := false
var label := Label.new()
var materials: Array[ShaderMaterial] = []
var current := 0
var at := START_AT
var distance := START_DISTANCE
var yaw := deg_to_rad(45.0) # looking from +x, +z, like the game's inspect renders
var pitch := deg_to_rad(-29.5)
var dragging := false


func _ready() -> void:
	_add_world()
	var terrain := MeshInstance3D.new()
	terrain.mesh = load("res://sample/terrain.res")
	add_child(terrain)
	water.mesh = load("res://sample/water.res")
	water.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(water)
	for entry in SHADERS:
		var material := ShaderMaterial.new()
		material.shader = load(entry[1])
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
	_show(0)
	_place_camera()
	if "--capture-liquids" in OS.get_cmdline_user_args():
		_capture()


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
	water.material_override = materials[current]
	_update_label()


func _update_label() -> void:
	label.text = "%d / %d  %s  (%s)\n%s\n1 - 6 or left / right: shader   N: day / night   drag: orbit   wheel: zoom   R: start view   F: whole map" % [
			current + 1, SHADERS.size(), SHADERS[current][0], "night" if night else "day", SHADERS[current][1]]


func _place_camera() -> void:
	var offset := Vector3(cos(pitch) * sin(yaw), -sin(pitch), cos(pitch) * cos(yaw)) * distance
	camera.position = at + offset
	camera.look_at(at)


func _unhandled_input(p_event: InputEvent) -> void:
	if p_event is InputEventKey and p_event.pressed and not p_event.echo:
		var key: int = p_event.keycode
		if key >= KEY_1 and key <= KEY_6:
			_show(key - KEY_1)
		elif key == KEY_RIGHT:
			_show(current + 1)
		elif key == KEY_LEFT:
			_show(current - 1)
		elif key == KEY_N:
			_set_night(not night)
		elif key == KEY_R:
			at = START_AT
			distance = START_DISTANCE
			yaw = deg_to_rad(45.0)
			pitch = deg_to_rad(-29.5)
		elif key == KEY_F:
			at = Vector3(16, 0, 16)
			distance = 30.0
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


# One shot of each shader from the start view, by day and by night, then quit.
func _capture() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	for at_night in [false, true]:
		_set_night(at_night)
		for i in SHADERS.size():
			_show(i)
			for f in 20:
				await get_tree().process_frame
			await RenderingServer.frame_post_draw
			var file: String = "liquid_%s%s.png" % [String(SHADERS[i][1]).get_file().get_basename(), "_night" if at_night else ""]
			get_viewport().get_texture().get_image().save_png(ProjectSettings.globalize_path("res://shots/" + file))
			print("saved shots/" + file)
	get_tree().quit()
