## The tuning panel (Tab, top right): pick a page — "wind-up" (shared by every attack: the wind
## point and swing planes) or a move — drag its sliders (the change applies to the next time it
## plays), then Save writes every page to MoveTuning.TUNING_PATH, which is loaded on start. Load
## re-reads the file; Defaults puts the picked page back to the code's values.
##
## Preview (the bar under the sliders): ▶ plays the picked move (the wind-up page plays a throw) at a
## set target from a fresh scene (MovePreview), ❚❚ pauses / goes on, the speed slows it down, the
## timeline scrubs to any moment of it, ■ ends the preview and gives the scene back. Dragging a slider
## while a preview is on replays it to the same moment, so the curves (C) change live as you drag.
## ⟲ View puts the editor camera (OrbitView: orbit / pan / zoom with the mouse) back to the game's view.
class_name TuningPanel
extends PanelContainer

const WIDTH := 420.0

var _tuning: MoveTuning
var _picker := OptionButton.new()
var _rows := VBoxContainer.new()
var _status := Label.new()
var _sections: Array[String] = [] ## the picker's items, in order
var _preview: MovePreview
var _timeline := HSlider.new()
var _clock := Label.new()
var _scrubbing := false ## the timeline is being moved by the code, not by you
var _reset_view: Callable


func setup(
		tuning: MoveTuning,
		preview: MovePreview,
		reset_view: Callable
) -> void:
	_tuning = tuning
	_preview = preview
	_reset_view = reset_view
	visible = false
	custom_minimum_size.x = WIDTH
	var box := VBoxContainer.new()
	add_child(box)
	var top := HBoxContainer.new()
	box.add_child(top)
	_picker.focus_mode = Control.FOCUS_NONE
	_picker.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for section: String in tuning.targets:
		_sections.append(section)
		_picker.add_item("wind-up" if section == "shared" else section)
	_picker.item_selected.connect(func(_index: int) -> void: _rebuild())
	top.add_child(_picker)
	for button: Array in [["Save", _save], ["Load", _load], ["Defaults", _defaults]]:
		var b := Button.new()
		b.text = button[0]
		b.focus_mode = Control.FOCUS_NONE
		b.pressed.connect(button[1])
		top.add_child(b)
	box.add_child(_rows)
	box.add_child(_preview_bar())
	var scrub := HBoxContainer.new()
	_timeline.min_value = 0.0
	_timeline.max_value = MovePreview.MAX_TIME
	_timeline.step = MovePreview.STEP
	_timeline.focus_mode = Control.FOCUS_NONE
	_timeline.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_timeline.value_changed.connect(func(v: float) -> void:
		if not _scrubbing:
			_preview.scrub(v))
	_clock.custom_minimum_size.x = 110
	scrub.add_child(_timeline)
	scrub.add_child(_clock)
	box.add_child(scrub)
	box.add_child(_status)
	_rebuild()


## Keep it in the top-right corner; the timeline follows the preview.
func _process(_delta: float) -> void:
	position = Vector2(get_viewport().get_visible_rect().size.x - WIDTH - 16.0, 70.0)
	var end := _preview.length if _preview.length > 0.0 else MovePreview.MAX_TIME
	_scrubbing = true
	_timeline.max_value = maxf(end, _preview.time)
	_timeline.value = _preview.time
	_scrubbing = false
	_clock.text = "%.2f / %s s" % [_preview.time, "%.2f" % _preview.length if _preview.length > 0.0 else "?"] if _preview.active else "no preview"


# ▶ ❚❚ ■ and the speed.
func _preview_bar() -> HBoxContainer:
	var bar := HBoxContainer.new()
	for button: Array in [["▶ Play", _play], ["❚❚ Pause", _preview.toggle], ["■ Stop", _preview.stop], ["⟲ View", _reset_view]]:
		var b := Button.new()
		b.text = button[0]
		b.focus_mode = Control.FOCUS_NONE
		b.pressed.connect(button[1])
		bar.add_child(b)
	var speed := OptionButton.new()
	speed.focus_mode = Control.FOCUS_NONE
	for factor in [1.0, 0.5, 0.25, 0.1]:
		speed.add_item("%sx" % factor)
	speed.item_selected.connect(func(index: int) -> void: _preview.speed = [1.0, 0.5, 0.25, 0.1][index])
	bar.add_child(speed)
	return bar


# Play the picked page's move (the wind-up page: a throw).
func _play() -> void:
	var section := _section()
	_preview.play("throw" if section == "shared" else section)


func _section() -> String:
	return _sections[_picker.selected]


# One slider per tunable of the picked move, showing its current value.
func _rebuild() -> void:
	for child in _rows.get_children():
		_rows.remove_child(child) # out of the layout now (freed at frame end)
		child.queue_free()
	var section := _section()
	var target: Object = _tuning.targets[section]
	for entry: Array in target.tunables():
		var name: String = entry[0]
		var whole: bool = float(entry[3]) >= 1.0
		var row := HBoxContainer.new()
		var label := Label.new()
		label.custom_minimum_size.x = 170
		var slider := HSlider.new()
		slider.min_value = entry[1]
		slider.max_value = entry[2]
		slider.step = entry[3]
		slider.value = float(target.get(name))
		slider.custom_minimum_size.x = 220
		slider.focus_mode = Control.FOCUS_NONE # keep the keys for the game
		label.text = _label(name, slider.value, whole)
		slider.value_changed.connect(func(v: float) -> void:
			_tuning.apply(section, name, v)
			label.text = _label(name, v, whole)
			_preview.refresh()) # a preview on: replay to the same moment, so the curve changes as you drag
		row.add_child(label)
		row.add_child(slider)
		_rows.add_child(row)
	reset_size.call_deferred() # shrink to fit the new rows


func _label(
		name: String,
		value: float,
		whole: bool
) -> String:
	return "%s  %s" % [name.replace("_", " "), "%d" % int(round(value)) if whole else "%.2f" % value]


func _save() -> void:
	var error := _tuning.save()
	_status.text = "saved to %s" % MoveTuning.TUNING_PATH if error == OK else "save failed (%s)" % error_string(error)


func _load() -> void:
	_status.text = "loaded" if _tuning.load_saved() else "no saved file yet"
	_rebuild()
	_preview.refresh()


func _defaults() -> void:
	_tuning.reset_section(_section())
	_status.text = "%s back to the code's values (not saved)" % _picker.get_item_text(_picker.selected)
	_rebuild()
	_preview.refresh()
