extends Node3D
## The simple waterfall built step by step, four tracks (T, or the panel's Track button, switches):
##   sheet  steps/*.gdshader on the waterfall sheet (the last step is waterfall_simple.gdshader)
##   pool   pool_steps/*.gdshader on the pool under it (the last is waterfall_pool.gdshader)
##   froth  froth_steps/*.gdshader on the tube at its foot (froth_tube.gd; the last is
##          waterfall_froth.gdshader)
##   stones stone_steps/*.gdshader on the stones scattered round it (stone_scatter.gd; the last
##          is waterfall_stone.gdshader)
## Each step file is a whole shader; the lines a step adds end in "// NEW". The panel shows the
## comment at the top of the step's file. Left / right (or Prev / Next): the step. Both start on
## their last step; the track not shown keeps the step it was on.

const StepHeader := preload("res://step_header.gd")
# [the panel's name, the folder of step files, the mesh they go on (or a node with a `material`
# that puts it on its meshes: the stones)]
const TRACKS := [
	["WATERFALL SHEET", "res://waterfall_simple/steps/", "Sheet/WaterfallSheet"],
	["POOL", "res://waterfall_simple/pool_steps/", "Pool/WaterfallPool"],
	["FROTH", "res://waterfall_simple/froth_steps/", "Froth"],
	["STONES", "res://waterfall_simple/stone_steps/", "Stones"],
]

var track := 0
var step_files := [] # per track: its step files, in order
var steps := [] # per track: the step it is on
var title := Label.new()
var notes := Label.new()
var track_button := Button.new()


func _ready() -> void:
	for t in TRACKS.size():
		var files: PackedStringArray = []
		for file in DirAccess.get_files_at(TRACKS[t][1]):
			if file.ends_with(".gdshader"):
				files.append(file)
		files.sort()
		step_files.append(files)
		steps.append(files.size() - 1)
	_add_panel()
	for t in range(TRACKS.size() - 1, -1, -1): # each mesh on its last step, the sheet shown
		track = t
		_show_step(steps[t])


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
		button.pressed.connect(func() -> void: _show_step(steps[track] + (1 if i == 1 else -1)))
		buttons.add_child(button)
	track_button.focus_mode = Control.FOCUS_NONE
	track_button.pressed.connect(_next_track)
	buttons.add_child(track_button)
	var keys := Label.new()
	keys.text = "left / right: step   T: track"
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


func _next_track() -> void:
	track = (track + 1) % TRACKS.size()
	_show_step(steps[track])


# Put step p_index of the current track on its mesh and show what it adds.
func _show_step(p_index: int) -> void:
	var files: PackedStringArray = step_files[track]
	if files.is_empty():
		return
	steps[track] = clampi(p_index, 0, files.size() - 1)
	var path: String = TRACKS[track][1] + files[steps[track]]
	var material := ShaderMaterial.new()
	material.shader = load(path)
	var target := get_node(TRACKS[track][2])
	if target is GeometryInstance3D:
		target.material_override = material
	else:
		target.set("material", material)
	var header := StepHeader.parse(FileAccess.get_file_as_string(path))
	title.text = "%s · %d / %d · %s" % [TRACKS[track][0], steps[track] + 1, files.size(), header[0]]
	notes.text = header[1]
	track_button.text = "Track (T): " + String(TRACKS[(track + 1) % TRACKS.size()][0]).to_lower()


func _unhandled_input(p_event: InputEvent) -> void:
	if p_event is InputEventKey and p_event.pressed and not p_event.echo:
		if p_event.keycode == KEY_RIGHT or p_event.keycode == KEY_LEFT:
			_show_step(steps[track] + (1 if p_event.keycode == KEY_RIGHT else -1))
			get_viewport().set_input_as_handled()
		elif p_event.keycode == KEY_T:
			_next_track()
			get_viewport().set_input_as_handled()
