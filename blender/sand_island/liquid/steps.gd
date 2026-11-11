extends "res://liquids.gd"

const StepHeader := preload("res://step_header.gd")
## The liquid shaders built step by step, one idea a step, on the same samples and light as the
## liquids viewer (liquids.gd). Two tracks (TRACKS): the mobile green poison, poison_mobile/steps/
## (step 12 is poison_mobile.gdshader), and the mobile water, water_mobile/steps/ (step 10 is
## water_mobile.gdshader: a cartoon look from the world normal and position, no UV).
## Each step file is a whole shader; the lines a step adds end in "// NEW". The panel shows the
## comment at the top of the step's file: what that step adds.
##
## Keys: left / right (or the panel's Prev / Next) previous / next step, T the other track, C the
## view of a step that has one (its show_channel uniform: step 4's flow channels), M the sample,
## N day / night, drag to orbit, wheel to zoom, R the start view, F the whole map.
## `-- --track=water` starts on the water. `-- --capture-steps` saves shots/<track>_step<NN>.png
## (day) for each step of each track and quits.

# [the panel's name, the folder of step files, the shots' prefix]
const TRACKS := [
	["POISON, MOBILE", "res://poison_mobile/steps/", "poison_mobile"],
	["WATER, MOBILE", "res://water_mobile/steps/", "water_mobile"],
]

var track := 0 # which of TRACKS
var step_files: PackedStringArray = [] # the track's step files, in order
var step := 0
var title := Label.new()
var notes := Label.new()
var channel := 0 # the step's show_channel (when it has one), kept while the next step has the same views
var channel_names: PackedStringArray = [] # its hint_enum names, empty when the step has none
var channel_button := Button.new()
var track_button := Button.new()


func _ready() -> void:
	super._ready() # the world, the samples, the camera
	label.visible = false # the shader list's label: the panel below instead
	_add_panel()
	_show_track(1 if "--track=water" in OS.get_cmdline_user_args() else 0)
	if "--capture-steps" in OS.get_cmdline_user_args():
		_capture_steps()


# Top left: the step's title, what it adds (small), Prev / Next and the keys.
func _add_panel() -> void:
	var panel := PanelContainer.new()
	panel.position = Vector2(14, 12)
	var box := VBoxContainer.new()
	box.custom_minimum_size = Vector2(620, 0)
	title.add_theme_font_size_override("font_size", 20)
	title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	notes.add_theme_font_size_override("font_size", 13)
	notes.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var buttons := HBoxContainer.new()
	for i in 2:
		var b := Button.new()
		b.text = ["Prev", "Next"][i]
		b.focus_mode = Control.FOCUS_NONE # keep the arrow keys for the steps
		b.pressed.connect(func() -> void: _show_step(step + (1 if i == 1 else -1)))
		buttons.add_child(b)
	track_button.focus_mode = Control.FOCUS_NONE
	track_button.pressed.connect(func() -> void: _show_track((track + 1) % TRACKS.size()))
	buttons.add_child(track_button)
	channel_button.focus_mode = Control.FOCUS_NONE
	channel_button.pressed.connect(_next_channel)
	buttons.add_child(channel_button)
	var keys := Label.new()
	keys.text = "left / right: step   T: track   C: view   M: sample   N: day / night   drag: orbit   wheel: zoom   R: start view   F: whole map"
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


# Load track p_index's step files and show its first step.
func _show_track(p_index: int) -> void:
	track = p_index
	step_files.clear()
	for file in DirAccess.get_files_at(TRACKS[track][1]):
		if file.ends_with(".gdshader"):
			step_files.append(file)
	step_files.sort()
	track_button.text = "Track (T): " + String(TRACKS[track][0]).to_lower()
	_show_step(0)


# Put step p_index on the liquid of every sample and show what it adds.
func _show_step(p_index: int) -> void:
	if step_files.is_empty():
		return
	step = clampi(p_index, 0, step_files.size() - 1)
	var path: String = TRACKS[track][1] + step_files[step]
	var material := ShaderMaterial.new()
	material.shader = load(path)
	var names := _channel_names(material.shader)
	if names != channel_names:
		channel = 0 # another step's views: start on its first (its normal look)
	channel_names = names
	if not channel_names.is_empty():
		material.set_shader_parameter("show_channel", channel)
	for liquid in liquids:
		liquid.material_override = material
	_update_channel_button()
	var header := _header_comment(FileAccess.get_file_as_string(path))
	title.text = "%s · %d / %d · %s" % [TRACKS[track][0], step + 1, step_files.size(), header[0]]
	notes.text = header[1]


# The names of a shader's show_channel views (its hint_enum), or none when it has no such uniform.
func _channel_names(p_shader: Shader) -> PackedStringArray:
	for uniform in p_shader.get_shader_uniform_list():
		if uniform["name"] == "show_channel":
			return String(uniform["hint_string"]).split(",")
	return PackedStringArray()


# The next view of the step (C, or the panel's button); nothing when the step has none.
func _next_channel() -> void:
	if channel_names.is_empty():
		return
	channel = (channel + 1) % channel_names.size()
	var material := liquids[0].material_override as ShaderMaterial
	material.set_shader_parameter("show_channel", channel)
	_update_channel_button()


func _update_channel_button() -> void:
	channel_button.visible = not channel_names.is_empty()
	if not channel_names.is_empty():
		channel_button.text = "View (C): " + channel_names[channel]


# The liquids viewer's shader keys (1 - 7) would put a finished shader on: keep the step's.
func _show(_p_index: int) -> void:
	_show_step(step)


# The leading // comment of a shader file: [its title, what it adds] (step_header.gd).
func _header_comment(p_code: String) -> Array:
	return StepHeader.parse(p_code)


func _unhandled_input(p_event: InputEvent) -> void:
	if p_event is InputEventKey and p_event.pressed and not p_event.echo:
		if p_event.keycode == KEY_RIGHT or p_event.keycode == KEY_LEFT:
			_show_step(step + (1 if p_event.keycode == KEY_RIGHT else -1))
			get_viewport().set_input_as_handled()
			return
		if p_event.keycode == KEY_T:
			_show_track((track + 1) % TRACKS.size())
			get_viewport().set_input_as_handled()
			return
		if p_event.keycode == KEY_C:
			_next_channel()
			get_viewport().set_input_as_handled()
			return
	super._unhandled_input(p_event)


# One shot of each step of each track from the MC sample's start view, by day, then quit.
func _capture_steps() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	var ignore := FileAccess.open("res://shots/.gdignore", FileAccess.WRITE) # Godot: don't import the shots
	ignore.close()
	_show_sample(0)
	_set_night(false)
	for t in TRACKS.size():
		_show_track(t)
		for i in step_files.size():
			_show_step(i)
			for f in 20:
				await get_tree().process_frame
			await RenderingServer.frame_post_draw
			var file := "%s_%s.png" % [TRACKS[t][2], step_files[i].get_basename()]
			get_viewport().get_texture().get_image().save_png(ProjectSettings.globalize_path("res://shots/" + file))
			print("saved shots/" + file)
	get_tree().quit()
