## The hammer demo's tuning panel (Tab): a slider for every number of the attack (SmashAttack
## .tunables), and the timeline: scrub it to see any moment of the last attack (it pauses there),
## ▶ replays it, ❚❚ pauses / goes on, and the speed slows it down. A slider moved while paused
## shows its effect at once (the attack is re-sampled at the same moment).
class_name HammerPanel
extends PanelContainer

const WIDTH := 400.0

var _demo: HammerDemo
var _timeline := HSlider.new()
var _clock := Label.new()
var _scrubbing := false ## the code is moving the timeline, not you


func setup(demo: HammerDemo) -> void:
	_demo = demo
	visible = false
	custom_minimum_size.x = WIDTH
	var box := VBoxContainer.new()
	add_child(box)
	var attack := demo.attack
	for entry: Array in attack.tunables():
		box.add_child(_slider_row(attack, entry))
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


func _process(_delta: float) -> void:
	position = Vector2(get_viewport().get_visible_rect().size.x - WIDTH - 16.0, 70.0)
	var attack := _demo.attack
	_scrubbing = true
	_timeline.max_value = attack.length()
	_timeline.value = clampf(_demo.time, 0.0, attack.length())
	_scrubbing = false
	_clock.text = "%.2f / %.2f s" % [clampf(_demo.time, 0.0, attack.length()), attack.length()]


func _toggle() -> void:
	if _demo.target >= 0:
		_demo.paused = not _demo.paused


func _slider_row(
		target: Object,
		entry: Array
) -> HBoxContainer:
	var name: String = entry[0]
	var row := HBoxContainer.new()
	var label := Label.new()
	label.custom_minimum_size.x = 170
	var slider := HSlider.new()
	slider.min_value = entry[1]
	slider.max_value = entry[2]
	slider.step = entry[3]
	slider.value = float(target.get(name))
	slider.custom_minimum_size.x = 210
	slider.focus_mode = Control.FOCUS_NONE
	var digits := 0 if float(entry[3]) >= 1.0 else 2
	label.text = "%s  %s" % [name.replace("_", " "), String.num(slider.value, digits)]
	slider.value_changed.connect(func(v: float) -> void:
		target.set(name, v)
		label.text = "%s  %s" % [name.replace("_", " "), String.num(v, digits)])
	row.add_child(label)
	row.add_child(slider)
	return row
