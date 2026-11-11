extends Node3D
## The simple waterfall sheet built step by step: steps/*.gdshader, each a whole shader (the lines a
## step adds end in "// NEW"); the last one is waterfall_simple.gdshader. The panel shows the
## comment at the top of the step's file. Left / right (or Prev / Next): the step. It starts on the
## last step.

const StepHeader := preload("res://step_header.gd")
const STEPS := "res://waterfall_simple/steps/"

@onready var sheet: MeshInstance3D = $Sheet/WaterfallSheet

var step_files: PackedStringArray = []
var step := 0
var title := Label.new()
var notes := Label.new()


func _ready() -> void:
	for file in DirAccess.get_files_at(STEPS):
		if file.ends_with(".gdshader"):
			step_files.append(file)
	step_files.sort()
	_add_panel()
	_show_step(step_files.size() - 1)


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
	keys.text = "left / right: step"
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


func _show_step(p_index: int) -> void:
	if step_files.is_empty():
		return
	step = clampi(p_index, 0, step_files.size() - 1)
	var path := STEPS + step_files[step]
	var material := ShaderMaterial.new()
	material.shader = load(path)
	sheet.material_override = material
	var header := StepHeader.parse(FileAccess.get_file_as_string(path))
	title.text = "WATERFALL SHEET · %d / %d · %s" % [step + 1, step_files.size(), header[0]]
	notes.text = header[1]


func _unhandled_input(p_event: InputEvent) -> void:
	if p_event is InputEventKey and p_event.pressed and not p_event.echo:
		if p_event.keycode == KEY_RIGHT or p_event.keycode == KEY_LEFT:
			_show_step(step + (1 if p_event.keycode == KEY_RIGHT else -1))
			get_viewport().set_input_as_handled()
