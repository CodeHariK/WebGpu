## The hammer demo's tuning panel (Tab). Tabs of sliders, every one applied live:
##   Hammer         its size (reach, head radius / length, handle thickness), where the hands grip
##                  it (how far the handle sticks out past the right hand, the gap to the left hand)
##   Idle pose      where the hammer and the hands are standing (hand radius and angle, the swing
##                  plane's side, the hammer's angle)
##   Swing          the attack's angles, lean and twist
##   Jump & timing  the attack's times, the jump, the squashes
##   Body           the body's own sizes (KingClown: pelvis, spine, shoulders, arm and leg lengths…)
## Save writes them all to HammerTuning.PATH (loaded on start), Load reads it back, Defaults puts the
## picked tab back to the code's values.
##
## Under the tabs, the timeline: scrub it to see any moment of the last attack (it pauses there),
## ▶ replays it, ❚❚ pauses / goes on, and the speed slows it down. A slider moved while paused shows
## its effect at once (the attack is re-sampled at the same moment).
class_name HammerPanel
extends PanelContainer

const WIDTH := 430.0

var _demo: HammerDemo
var _tabs := TabContainer.new()
var _timeline := HSlider.new()
var _clock := Label.new()
var _status := Label.new()
var _scrubbing := false ## the code is moving the timeline, not you


func setup(demo: HammerDemo) -> void:
	_demo = demo
	visible = false
	custom_minimum_size.x = WIDTH
	var box := VBoxContainer.new()
	add_child(box)
	var top := HBoxContainer.new()
	for button: Array in [["Save", _save], ["Load", _load], ["Defaults", _defaults]]:
		var b := Button.new()
		b.text = button[0]
		b.focus_mode = Control.FOCUS_NONE
		b.pressed.connect(button[1])
		top.add_child(b)
	top.add_child(_status)
	box.add_child(top)
	_tabs.focus_mode = Control.FOCUS_NONE
	box.add_child(_tabs)
	var bar := HBoxContainer.new()
	for button: Array in [["▶ Play", demo.replay], ["❚❚ Pause", _toggle]]:
		var b := Button.new()
		b.text = button[0]
		b.focus_mode = Control.FOCUS_NONE
		b.pressed.connect(button[1])
		bar.add_child(b)
	var speed := OptionButton.new()
	speed.focus_mode = Control.FOCUS_NONE
	for factor in [1.0, 0.5, 0.25, 0.1]:
		speed.add_item("%sx" % factor)
	speed.item_selected.connect(func(index: int) -> void: _demo.speed = [1.0, 0.5, 0.25, 0.1][index])
	bar.add_child(speed)
	box.add_child(bar)
	var scrub := HBoxContainer.new()
	_timeline.step = 1.0 / 120.0
	_timeline.focus_mode = Control.FOCUS_NONE
	_timeline.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_timeline.value_changed.connect(func(v: float) -> void:
		if not _scrubbing:
			_demo.scrub(v))
	_clock.custom_minimum_size.x = 90
	scrub.add_child(_timeline)
	scrub.add_child(_clock)
	box.add_child(scrub)
	rebuild()


## The tabs again (the body changed, or the values were loaded / reset).
func rebuild() -> void:
	var keep := _tabs.current_tab
	for child in _tabs.get_children():
		_tabs.remove_child(child)
		child.queue_free()
	_add_tabs(_demo.attack, _demo.attack.tunables())
	_add_tabs(_demo.body, _demo.body.tunables())
	if keep >= 0 and keep < _tabs.get_tab_count():
		_tabs.current_tab = keep


func _process(_delta: float) -> void:
	var view := get_viewport().get_visible_rect().size
	position = Vector2(view.x - WIDTH - 16.0, 70.0)
	for tab in _tabs.get_children(): # the sliders scroll; the panel fits the window
		(tab as ScrollContainer).custom_minimum_size.y = clampf(view.y - 330.0, 150.0, 520.0)
	var attack := _demo.attack
	_scrubbing = true
	_timeline.max_value = attack.length()
	_timeline.value = clampf(_demo.time, 0.0, attack.length())
	_scrubbing = false
	_clock.text = "%.2f / %.2f s" % [clampf(_demo.time, 0.0, attack.length()), attack.length()]


# One tab per heading in `entries` (a String), its sliders below it.
func _add_tabs(
		target: Object,
		entries: Array
) -> void:
	var rows: VBoxContainer = null
	for entry in entries:
		if entry is String:
			var scroll := ScrollContainer.new()
			scroll.name = entry
			scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
			rows = VBoxContainer.new()
			rows.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			scroll.add_child(rows)
			_tabs.add_child(scroll)
			scroll.set_meta("target", target)
			continue
		rows.add_child(_slider_row(target, entry))


func _toggle() -> void:
	if _demo.time >= 0.0:
		_demo.paused = not _demo.paused


func _save() -> void:
	var error := HammerTuning.save(_demo.attack, _demo.bodies)
	_status.text = "saved" if error == OK else "save failed (%s)" % error_string(error)


func _load() -> void:
	_status.text = "loaded" if HammerTuning.load_into(_demo.attack, _demo.bodies) else "no saved file yet"
	rebuild()


func _defaults() -> void:
	var tab := _tabs.get_current_tab_control()
	if tab == null:
		return
	HammerTuning.reset(tab.get_meta("target"), tab.name)
	_status.text = "%s back to the code's values (not saved)" % tab.name
	rebuild()


func _slider_row(
		target: Object,
		entry: Array
) -> HBoxContainer:
	var name: String = entry[0]
	var row := HBoxContainer.new()
	var label := Label.new()
	label.custom_minimum_size.x = 180
	var slider := HSlider.new()
	slider.min_value = entry[1]
	slider.max_value = entry[2]
	slider.step = entry[3]
	slider.value = float(target.get(name))
	slider.custom_minimum_size.x = 200
	slider.focus_mode = Control.FOCUS_NONE
	var digits := 0 if float(entry[3]) >= 1.0 else (3 if float(entry[3]) < 0.01 else 2)
	label.text = "%s  %s" % [name.replace("_", " "), String.num(slider.value, digits)]
	slider.value_changed.connect(func(v: float) -> void:
		target.set(name, v)
		label.text = "%s  %s" % [name.replace("_", " "), String.num(v, digits)])
	row.add_child(label)
	row.add_child(slider)
	return row
