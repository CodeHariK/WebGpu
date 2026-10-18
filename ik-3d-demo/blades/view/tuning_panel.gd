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
## With the curves on (C) and a preview on, left-drag a curve's control point to bend the move there
## (CurveEditor); Save keeps the bends, Defaults on a move's page removes them.
##
## Timing (under the sliders): pick one of the move's phases (they appear once it has played in a
## preview) and change how long it takes (× time) and its speed curve (TimingCurve: drag the dots).
## The timeline shows when each phase starts and each hit lands (TimelineMarks).
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
var _phase_pick := OptionButton.new() ## the timing editor's phase ("pull/yank" …)
var _phase_keys: Array[String] = []
var _time_scale := HSlider.new()
var _time_label := Label.new()
var _curve := TimingCurve.new()
var _marks := TimelineMarks.new()
var _showing := false ## the timing widgets are being set by the code, not by you


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
	box.add_child(_timing_box())
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
	var line := VBoxContainer.new() # the marks right above the timeline, the same width
	line.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	line.add_theme_constant_override("separation", 0)
	line.add_child(_marks)
	line.add_child(_timeline)
	scrub.add_child(line)
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
	_marks.show_marks(_preview.marks if _preview.move == _move() else [], _timeline.max_value)
	_curve.set_playhead(_phase_progress())
	if _tuning.timing.phases_of(_move()).size() != _phase_keys.size():
		_fill_phases() # the move has played a new phase: list it
	_clock.text = "%.2f / %s s" % [_preview.time, "%.2f" % _preview.length if _preview.length > 0.0 else "?"] if _preview.active else "no preview"


# The timing editor: phase picker, × time, reset, and the speed curve.
func _timing_box() -> VBoxContainer:
	var box := VBoxContainer.new()
	var top := HBoxContainer.new()
	var title := Label.new()
	title.text = "timing"
	top.add_child(title)
	_phase_pick.focus_mode = Control.FOCUS_NONE
	_phase_pick.custom_minimum_size.x = 120
	_phase_pick.item_selected.connect(func(_index: int) -> void: _show_phase())
	top.add_child(_phase_pick)
	_time_scale.min_value = 0.25
	_time_scale.max_value = 3.0
	_time_scale.step = 0.05
	_time_scale.value = 1.0
	_time_scale.focus_mode = Control.FOCUS_NONE
	_time_scale.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_time_scale.custom_minimum_size.x = 110
	_time_scale.value_changed.connect(_set_scale)
	_time_label.custom_minimum_size.x = 64
	top.add_child(_time_label)
	top.add_child(_time_scale)
	var reset := Button.new()
	reset.text = "Reset"
	reset.focus_mode = Control.FOCUS_NONE
	reset.pressed.connect(_reset_phase)
	top.add_child(reset)
	box.add_child(top)
	_curve.changed.connect(_set_curve)
	box.add_child(_curve)
	return box


# The move whose phases the timing editor lists (the wind-up page: a throw's).
func _move() -> String:
	return "throw" if _section() == "shared" else _section()


func _fill_phases() -> void:
	var keep := _phase_key()
	_phase_keys = _tuning.timing.phases_of(_move())
	_phase_pick.clear()
	for key in _phase_keys:
		_phase_pick.add_item(key.get_slice("/", 1))
	if _phase_keys.is_empty():
		_phase_pick.add_item("play it first")
	elif keep in _phase_keys:
		_phase_pick.select(_phase_keys.find(keep))
	_show_phase()


# The picked phase's progress (0..1) on the first blade playing it in the preview; −1: none is.
func _phase_progress() -> float:
	var key := _phase_key()
	if key == "" or not _preview.active:
		return -1.0
	for blade: Blade in _preview.blades():
		if blade.delay <= 0.0 and PhaseTiming.key_of(blade.move, blade.phase) == key:
			return blade.progress()
	return -1.0


func _phase_key() -> String:
	var index := _phase_pick.selected
	return _phase_keys[index] if index >= 0 and index < _phase_keys.size() else ""


# Put the picked phase's edits on the widgets.
func _show_phase() -> void:
	var key := _phase_key()
	var timing := _tuning.timing
	_showing = true
	_time_scale.editable = key != ""
	_time_scale.value = float(timing.scales.get(key, 1.0))
	_showing = false
	_time_label.text = "× %.2f" % _time_scale.value
	var seen: Dictionary = timing.seen.get(key, {"easing": "linear", "time": 0.0})
	_time_label.tooltip_text = "%.2f s as the move makes it" % float(seen.time)
	_curve.show_phase(seen.easing, timing.curves.get(key))


func _set_scale(value: float) -> void:
	_time_label.text = "× %.2f" % value
	var key := _phase_key()
	if _showing or key == "":
		return
	if is_equal_approx(value, 1.0):
		_tuning.timing.scales.erase(key)
	else:
		_tuning.timing.scales[key] = value
	_preview.refresh()


func _set_curve(handles: Vector4) -> void:
	var key := _phase_key()
	if key == "":
		return
	_tuning.timing.curves[key] = handles
	_preview.refresh()


func _reset_phase() -> void:
	var key := _phase_key()
	_tuning.timing.curves.erase(key)
	_tuning.timing.scales.erase(key)
	_show_phase()
	_preview.refresh()


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
		var digits := 0 if float(entry[3]) >= 1.0 else (3 if float(entry[3]) < 0.01 else 2) # decimals the step needs
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
		label.text = _label(name, slider.value, digits)
		slider.value_changed.connect(func(v: float) -> void:
			_tuning.apply(section, name, v)
			label.text = _label(name, v, digits)
			_preview.refresh()) # a preview on: replay to the same moment, so the curve changes as you drag
		row.add_child(label)
		row.add_child(slider)
		_rows.add_child(row)
	_fill_phases()
	reset_size.call_deferred() # shrink to fit the new rows


func _label(
		name: String,
		value: float,
		digits: int
) -> String:
	return "%s  %s" % [name.replace("_", " "), String.num(value, digits)]


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
