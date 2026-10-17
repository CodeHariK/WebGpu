## Sliders (bottom left) for the wind-up and the swing planes; they change BladesDemo's settings and
## apply to the next attack.
class_name BladeSliders
extends RefCounted


static func build(
		demo: BladesDemo,
		hud: CanvasLayer
) -> void:
	var panel := PanelContainer.new()
	panel.position = Vector2(16, demo.get_viewport().get_visible_rect().size.y - 200)
	var box := VBoxContainer.new()
	panel.add_child(box)
	hud.add_child(panel)
	_slider(box, "Wind-up behind", 0.0, 3.0, 0.05, demo.top_behind, "%.2f m", func(v: float) -> void: demo.top_behind = v)
	_slider(box, "Wind-up side", 0.0, 2.0, 0.05, demo.wind_side, "%.2f m", func(v: float) -> void: demo.wind_side = v)
	_slider(box, "Wind-up height", 1.4, 4.5, 0.05, demo.top_height, "%.2f m", func(v: float) -> void: demo.top_height = v)
	_slider(box, "Wind-up time", 0.04, 0.6, 0.01, demo.windup_time, "%.2f s", func(v: float) -> void: demo.windup_time = v)
	_slider(box, "Swing tilt (out)", 0.0, 80.0, 1.0, demo.swing_tilt, "%.0f°", func(v: float) -> void: demo.swing_tilt = v)


static func _slider(
		box: VBoxContainer,
		title: String,
		low: float,
		high: float,
		step: float,
		value: float,
		format: String,
		apply: Callable
) -> void:
	var row := HBoxContainer.new()
	var label := Label.new()
	label.custom_minimum_size.x = 200
	label.text = "%s  %s" % [title, format % value]
	var slider := HSlider.new()
	slider.min_value = low
	slider.max_value = high
	slider.step = step
	slider.value = value
	slider.custom_minimum_size.x = 220
	slider.focus_mode = Control.FOCUS_NONE # keep the keys for the game
	slider.value_changed.connect(func(v: float) -> void:
		apply.call(v)
		label.text = "%s  %s" % [title, format % v])
	row.add_child(label)
	row.add_child(slider)
	box.add_child(row)
