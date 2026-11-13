extends Node3D
## The whirlpool built step by step (steps/*.gdshader; the last step is whirlpool.gdshader) on one
## flat sheet: a PlaneMesh SIZE metres square cut into SQUARES x SQUARES squares (0.12 m: dense
## enough for the funnel the vertex shader sinks from step 8). No flow map and no UV: the shader
## works everything out from the world position, measured from the sheet's origin (move the sheet
## and the whirlpool goes with it).
## Each step file is a whole shader; the lines a step adds end in "// NEW". The panel shows the
## comment at the top of the step's file.
##
## Keys: left / right (or the panel's Prev / Next) the step, drag to orbit, wheel to zoom.
## `-- --capture-steps` saves shots/whirlpool_step<NN>_<name>.png for each step and quits.

const StepHeader := preload("res://step_header.gd")
const STEPS := "res://whirlpool/steps/"
const SIZE := 24.0 # metres across the sheet
const SQUARES := 200 # along each side

var water := MeshInstance3D.new()
var camera := Camera3D.new()
var files: PackedStringArray = []
var step := 0
var title := Label.new()
var notes := Label.new()
var yaw := deg_to_rad(20.0)
var pitch := deg_to_rad(-38.0)
var distance := 13.0
var dragging := false


func _ready() -> void:
	_add_world()
	_add_water()
	_add_panel()
	camera.fov = 60.0
	add_child(camera)
	_place_camera()
	for file in DirAccess.get_files_at(STEPS):
		if file.ends_with(".gdshader"):
			files.append(file)
	files.sort()
	_show_step(files.size() - 1)
	if "--capture-steps" in OS.get_cmdline_user_args():
		_capture_steps()


# Sun (with shadows), sky and the game's filmic look.
func _add_world() -> void:
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-45, 70, 0) # from the side: its glint off the water stays out of the start view
	sun.shadow_enabled = true
	add_child(sun)
	var sky := Sky.new()
	sky.sky_material = ProceduralSkyMaterial.new()
	var env := Environment.new()
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	var world := WorldEnvironment.new()
	world.environment = env
	add_child(world)


# The flat sheet the shader goes on, its origin in the middle (the whirlpool's middle).
func _add_water() -> void:
	var plane := PlaneMesh.new()
	plane.size = Vector2(SIZE, SIZE)
	plane.subdivide_width = SQUARES - 1 # cuts, not squares
	plane.subdivide_depth = SQUARES - 1
	water.mesh = plane
	water.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(water)


# Top left: the step's title, what it adds (small), Prev / Next and the keys.
func _add_panel() -> void:
	var panel := PanelContainer.new()
	panel.position = Vector2(14, 12)
	var box := VBoxContainer.new()
	box.custom_minimum_size = Vector2(560, 0)
	title.add_theme_font_size_override("font_size", 20)
	title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	notes.add_theme_font_size_override("font_size", 13)
	notes.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var buttons := HBoxContainer.new()
	for i in 2:
		var button := Button.new()
		button.text = ["Prev", "Next"][i]
		button.focus_mode = Control.FOCUS_NONE # keep the arrow keys for the steps
		button.pressed.connect(func() -> void: _show_step(step + (1 if i == 1 else -1)))
		buttons.add_child(button)
	var keys := Label.new()
	keys.text = "left / right: step   drag: orbit   wheel: zoom"
	keys.add_theme_font_size_override("font_size", 12)
	keys.modulate = Color(1, 1, 1, 0.6)
	box.add_child(title)
	box.add_child(notes)
	box.add_child(buttons)
	box.add_child(keys)
	panel.add_child(box)
	var layer := CanvasLayer.new()
	layer.add_child(panel)
	add_child(layer)


# Put step p_index on the sheet and show what it adds.
func _show_step(p_index: int) -> void:
	if files.is_empty():
		return
	step = clampi(p_index, 0, files.size() - 1)
	var path := STEPS + files[step]
	var material := ShaderMaterial.new()
	material.shader = load(path)
	water.material_override = material
	var header := StepHeader.parse(FileAccess.get_file_as_string(path))
	title.text = "WHIRLPOOL · %d / %d · %s" % [step + 1, files.size(), header[0]]
	notes.text = header[1]


func _place_camera() -> void:
	camera.position = Vector3(cos(pitch) * sin(yaw), -sin(pitch), cos(pitch) * cos(yaw)) * distance
	camera.look_at(Vector3.ZERO)


func _unhandled_input(p_event: InputEvent) -> void:
	if p_event is InputEventKey and p_event.pressed and not p_event.echo:
		if p_event.keycode == KEY_RIGHT or p_event.keycode == KEY_LEFT:
			_show_step(step + (1 if p_event.keycode == KEY_RIGHT else -1))
	elif p_event is InputEventMouseButton:
		if p_event.button_index == MOUSE_BUTTON_LEFT:
			dragging = p_event.pressed
		elif p_event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = max(distance * 0.9, 2.0)
		elif p_event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = min(distance * 1.1, 40.0)
	elif p_event is InputEventMouseMotion and dragging:
		yaw -= p_event.relative.x * 0.008
		pitch = clamp(pitch - p_event.relative.y * 0.008, deg_to_rad(-89.0), deg_to_rad(-5.0))
	_place_camera()


# One shot of each step, 3 seconds in (so the turning steps have turned), then quit.
func _capture_steps() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	for i in files.size():
		_show_step(i)
		await get_tree().create_timer(3.0).timeout
		await RenderingServer.frame_post_draw
		var file := "whirlpool_%s.png" % files[i].get_basename()
		get_viewport().get_texture().get_image().save_png(ProjectSettings.globalize_path("res://shots/" + file))
		print("saved shots/" + file)
	get_tree().quit()
