class_name GrassSliders
extends VBoxContainer
## Sliders for the grass, shown in the debug panel:
##   near / far density   blades / tufts a square metre (the tiles' shared pattern is rebuilt)
##   wind strength        how far a gust leans the grass (x its height)
##   wind speed           how fast the gusts cross (noise texture widths a second)
##   colour variation     0 every blade alike .. 3 three times the spread of shade and dryness
##   dryness              -1 all green .. 1 all dry, on top of the map's

## [label, min, max, step, start]
const SLIDERS := [
	["near density", 0.0, 60.0, 1.0, 28.0],
	["far density", 0.0, 8.0, 0.25, 3.0],
	["wind strength", 0.0, 2.0, 0.05, 0.6],
	["wind speed", 0.0, 0.3, 0.01, 0.05],
	["colour variation", 0.0, 3.0, 0.1, 1.0],
	["dryness", -1.0, 1.0, 0.05, 0.0],
]
const FONT_SIZE := 11

var grass: GrassField


func _ready() -> void:
	for spec: Array in SLIDERS:
		var label := Label.new()
		label.add_theme_font_size_override(&"font_size", FONT_SIZE)
		add_child(label)
		var slider := HSlider.new()
		slider.min_value = spec[1]
		slider.max_value = spec[2]
		slider.step = spec[3]
		slider.value = spec[4]
		slider.focus_mode = Control.FOCUS_NONE # keep the arrow keys for the camera
		slider.custom_minimum_size = Vector2(220, 0)
		slider.value_changed.connect(func(p_value: float) -> void: _apply(spec[0], p_value, label))
		add_child(slider)
		_apply(spec[0], spec[4], label)


func _apply(p_name: String, p_value: float, p_label: Label) -> void:
	p_label.text = "%s: %s" % [p_name, str(snappedf(p_value, 0.01))]
	match p_name:
		"near density":
			grass.set_near_density(p_value)
		"far density":
			grass.set_far_density(p_value)
		"wind strength":
			grass.set_shared(&"wind_strength", p_value)
		"wind speed":
			grass.set_shared(&"wind_speed", p_value)
		"colour variation":
			grass.set_shared(&"color_variation", p_value)
		"dryness":
			grass.set_shared(&"dryness", p_value)
