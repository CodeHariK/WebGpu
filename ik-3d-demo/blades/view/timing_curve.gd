## The timing editor's curve: a phase's speed curve, time across (0 → 1), distance along the path up
## (0 → 1). Grey: the curve the move has (its easing); orange: your edit, a cubic Bezier from the
## bottom-left to the top-right corner whose two handles (the dots) you drag. Dragging a handle on a
## phase with no edit yet starts one from the move's curve. Steep = fast, flat = slow.
## While the preview plays that phase, a white dot rides the curve at the blade's moment in it (a
## line marks the time), so you see where on the curve it is fast or slow.
class_name TimingCurve
extends Control

const HANDLE_PIXELS := 12.0 ## a handle this close to the mouse can be grabbed
const PAD := 10.0 ## pixels round the unit square (a handle may go a little outside it)
const Y_RANGE := Vector2(-0.3, 1.3) ## the handles' height can go this far (overshoot / pull back)

signal changed(handles: Vector4) ## a handle was dragged: the new handles

var easing := "linear" ## the move's own easing (drawn grey)
var handles := PhaseTiming.LINEAR
var edited := false ## an edit exists (draw it, else just the move's curve)
var playhead := -1.0 ## the blade's time in this phase (0..1); −1: the phase isn't playing

var _drag := -1 ## the handle being dragged (0, 1), −1: none


func _init() -> void:
	custom_minimum_size = Vector2(0, 150)
	mouse_filter = Control.MOUSE_FILTER_STOP


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.1, 0.1, 0.12))
	for i in 5: # a grid every quarter
		var f := i / 4.0
		draw_line(_to_screen(Vector2(f, 0)), _to_screen(Vector2(f, 1)), Color(1, 1, 1, 0.06))
		draw_line(_to_screen(Vector2(0, f)), _to_screen(Vector2(1, f)), Color(1, 1, 1, 0.06))
	draw_polyline(_points(func(t: float) -> float: return Blade.eased_time(t, easing)), Color(0.6, 0.6, 0.6, 0.7), 2.0)
	if not edited:
		draw_string(get_theme_default_font(), Vector2(PAD + 4, PAD + 14), "%s — drag a dot to edit" % easing, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color(1, 1, 1, 0.5))
	var c := handles
	var orange := Color(1.0, 0.55, 0.15) if edited else Color(1.0, 0.55, 0.15, 0.35)
	if edited:
		draw_polyline(_points(func(t: float) -> float: return PhaseTiming.curve_at(t, c)), orange, 2.5)
	if playhead >= 0.0:
		var y := PhaseTiming.curve_at(playhead, c) if edited else Blade.eased_time(playhead, easing)
		var top := _to_screen(Vector2(playhead, Y_RANGE.y))
		draw_line(Vector2(top.x, PAD), Vector2(top.x, size.y - PAD), Color(1, 1, 1, 0.25), 1.0)
		draw_circle(_to_screen(Vector2(playhead, y)), 6.0, Color.WHITE)
	for i in 2:
		var corner := _to_screen(Vector2(0, 0) if i == 0 else Vector2(1, 1))
		var dot := _to_screen(Vector2(c.x, c.y) if i == 0 else Vector2(c.z, c.w))
		draw_line(corner, dot, orange, 1.0)
		draw_circle(dot, 5.0 if i != _drag else 7.0, orange)


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		_drag = _pick(event.position) if event.pressed else -1
		if _drag >= 0 and not edited: # the first drag starts from the move's own curve
			handles = PhaseTiming.PRESETS.get(easing, PhaseTiming.LINEAR)
			edited = true
		queue_redraw()
		accept_event()
	elif event is InputEventMouseMotion and _drag >= 0:
		var at := _to_unit(event.position)
		at.x = clampf(at.x, 0.0, 1.0) # time can't run backwards
		at.y = clampf(at.y, Y_RANGE.x, Y_RANGE.y)
		if _drag == 0:
			handles = Vector4(at.x, at.y, handles.z, handles.w)
		else:
			handles = Vector4(handles.x, handles.y, at.x, at.y)
		queue_redraw()
		changed.emit(handles)
		accept_event()


## Move the playhead (−1: hide it); redraws only when it changed.
func set_playhead(t: float) -> void:
	if not is_equal_approx(t, playhead):
		playhead = t
		queue_redraw()


## Show a phase: its move's easing and its edit (if any).
func show_phase(
		new_easing: String,
		edit: Variant
) -> void:
	easing = new_easing
	edited = edit != null
	handles = edit if edited else PhaseTiming.PRESETS.get(easing, PhaseTiming.LINEAR)
	queue_redraw()


func _pick(mouse: Vector2) -> int:
	for i in 2:
		var dot := _to_screen(Vector2(handles.x, handles.y) if i == 0 else Vector2(handles.z, handles.w))
		if dot.distance_to(mouse) < HANDLE_PIXELS:
			return i
	return -1


func _points(curve: Callable) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in 41:
		var t := i / 40.0
		out.append(_to_screen(Vector2(t, curve.call(t))))
	return out


# Unit square (y up, with room above and below for overshoot) → pixels, and back.
func _to_screen(unit: Vector2) -> Vector2:
	var box := _box()
	return Vector2(box.position.x + unit.x * box.size.x, box.end.y - (unit.y - Y_RANGE.x) / (Y_RANGE.y - Y_RANGE.x) * box.size.y)


func _to_unit(pixels: Vector2) -> Vector2:
	var box := _box()
	return Vector2((pixels.x - box.position.x) / box.size.x, Y_RANGE.x + (box.end.y - pixels.y) / box.size.y * (Y_RANGE.y - Y_RANGE.x))


func _box() -> Rect2:
	return Rect2(Vector2(PAD, PAD), size - Vector2(PAD, PAD) * 2.0)
