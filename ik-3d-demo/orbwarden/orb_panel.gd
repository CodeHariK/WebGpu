## Tuning panel for the Orb Warden demo (right side of the screen): a slider per number and a
## checkbox per switch, each bound to a getter / setter pair, so the panel holds no state of its
## own and always shows the live values (refreshed every frame, so keys and the panel agree).
## Controls never take keyboard focus: the keys keep driving the game while you use the panel.
class_name OrbPanel
extends PanelContainer

const WIDTH := 300.0
const ICON := 16 ## checkbox icon size (px): the default theme's unticked box is near invisible here
const BOX_EDGE := Color(0.75, 0.8, 0.85)
const BOX_TICK := Color(0.3, 0.9, 1.0)

static var _icons: Array[ImageTexture] = [] ## [unticked, ticked], made once

var _rows: Array = [] ## [Range or CheckBox, getter Callable, value Label or null, format String]
var _box: VBoxContainer


func _init() -> void:
	custom_minimum_size.x = WIDTH
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.05, 0.05, 0.07, 0.78)
	style.set_content_margin_all(10)
	style.set_corner_radius_all(6)
	add_theme_stylebox_override("panel", style)
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	add_child(scroll)
	_box = VBoxContainer.new()
	_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_box.custom_minimum_size.x = WIDTH - 32.0
	_box.add_theme_constant_override("separation", 2)
	scroll.add_child(_box)


func heading(text: String) -> void:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", 14)
	label.add_theme_color_override("font_color", Color(0.45, 0.9, 1.0))
	_box.add_child(label)


## A labelled slider from `low` to `high` in steps of `step`; `format` prints the value.
func slider(text: String, low: float, high: float, step: float, getter: Callable, setter: Callable, format := "%.2f") -> void:
	var top := HBoxContainer.new()
	var name_label := Label.new()
	name_label.text = text
	name_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	name_label.add_theme_font_size_override("font_size", 13)
	var value_label := Label.new()
	value_label.add_theme_font_size_override("font_size", 13)
	top.add_child(name_label)
	top.add_child(value_label)
	_box.add_child(top)
	var bar := HSlider.new()
	bar.min_value = low
	bar.max_value = high
	bar.step = step
	bar.focus_mode = Control.FOCUS_NONE
	bar.value = getter.call()
	bar.value_changed.connect(func(value: float) -> void: setter.call(value))
	_box.add_child(bar)
	_rows.append([bar, getter, value_label, format])


## A button that runs `action` (no state to show).
func button(text: String, action: Callable) -> void:
	var press := Button.new()
	press.text = text
	press.focus_mode = Control.FOCUS_NONE
	press.add_theme_font_size_override("font_size", 13)
	press.pressed.connect(func() -> void: action.call())
	_box.add_child(press)


func checkbox(text: String, getter: Callable, setter: Callable) -> void:
	var box := CheckBox.new()
	box.text = text
	box.focus_mode = Control.FOCUS_NONE
	var icons := _box_icons()
	box.add_theme_icon_override("unchecked", icons[0])
	box.add_theme_icon_override("checked", icons[1])
	box.add_theme_font_size_override("font_size", 13)
	box.button_pressed = getter.call()
	box.toggled.connect(func(on: bool) -> void: setter.call(on))
	_box.add_child(box)
	_rows.append([box, getter, null, ""])


# A light outlined square, and the same with a cyan fill inside (ticked).
static func _box_icons() -> Array[ImageTexture]:
	if _icons.is_empty():
		for ticked in [false, true]:
			var image := Image.create(ICON, ICON, false, Image.FORMAT_RGBA8)
			image.fill(Color(0, 0, 0, 0))
			for y in ICON:
				for x in ICON:
					var edge := x < 2 or y < 2 or x >= ICON - 2 or y >= ICON - 2
					var inside := x >= 4 and y >= 4 and x < ICON - 4 and y < ICON - 4
					if edge:
						image.set_pixel(x, y, BOX_EDGE)
					elif ticked and inside:
						image.set_pixel(x, y, BOX_TICK)
			_icons.append(ImageTexture.create_from_image(image))
	return _icons


# Show the live values (a key may have changed one); set_value_no_signal so nothing loops back.
func _process(_delta: float) -> void:
	for row: Array in _rows:
		var value: Variant = (row[1] as Callable).call()
		if row[0] is Range:
			var bar: Range = row[0]
			if not is_equal_approx(bar.value, value):
				bar.set_value_no_signal(value)
			(row[2] as Label).text = row[3] % value
		else:
			(row[0] as CheckBox).set_pressed_no_signal(value)
