## A minimal weapon HUD: a small crosshair dot in the middle (hidden while aiming down the sights)
## and the six chambers in the bottom-right corner, filled or empty. Drawn with _draw on one
## Control: no textures, redrawn only when something changes.
extends CanvasLayer

var rounds := 6
var capacity := 6
var aiming := false
var reloading := false

var _canvas := Control.new()


func _ready() -> void:
	_canvas.set_anchors_preset(Control.PRESET_FULL_RECT)
	_canvas.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_canvas.draw.connect(_draw_hud)
	add_child(_canvas)


func show_state(new_rounds: int, new_aiming: bool, new_reloading: bool) -> void:
	if new_rounds == rounds and new_aiming == aiming and new_reloading == reloading:
		return
	rounds = new_rounds
	aiming = new_aiming
	reloading = new_reloading
	_canvas.queue_redraw()


func _draw_hud() -> void:
	var size := _canvas.size
	if not aiming:
		_canvas.draw_circle(size / 2, 2.5, Color(1, 1, 1, 0.7))
		_canvas.draw_arc(size / 2, 2.5, 0, TAU, 12, Color(0, 0, 0, 0.5), 1.0)
	var centre := size - Vector2(70, 70)
	var ring := 26.0
	for i in capacity:
		var a := -PI / 2 + TAU * i / capacity
		var at := centre + Vector2(cos(a), sin(a)) * ring
		var full := i < rounds
		_canvas.draw_circle(at, 8.0, Color(0.85, 0.6, 0.2, 0.9) if full else Color(0, 0, 0, 0.35))
		_canvas.draw_arc(at, 8.0, 0, TAU, 16, Color(1, 1, 1, 0.5), 1.5)
	if reloading:
		_canvas.draw_string(ThemeDB.fallback_font, centre + Vector2(-34, 52), "reloading", HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(1, 1, 1, 0.8))
