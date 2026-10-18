## Above the preview's timeline: what happens when in the move (MovePreview.marks) — a tick and a
## label at each phase change ("out", "R hold" …), a red tick at each hit. Labels take turns on two
## rows so neighbours don't overlap. `end` is the timeline's right edge in seconds.
class_name TimelineMarks
extends Control

const GRABBER := 8.0 ## pixels the slider keeps free at each end (its grabber's half width)

var marks: Array = []
var end := 1.0


func _init() -> void:
	custom_minimum_size = Vector2(0, 30)
	mouse_filter = Control.MOUSE_FILTER_IGNORE


## New marks / range: redraw if they changed.
func show_marks(
		new_marks: Array,
		new_end: float
) -> void:
	if new_marks == marks and is_equal_approx(new_end, end):
		return
	marks = new_marks.duplicate()
	end = maxf(new_end, 0.01)
	queue_redraw()


func _draw() -> void:
	var font := get_theme_default_font()
	var row := 0
	for mark: Array in marks:
		var x := GRABBER + float(mark[0]) / end * (size.x - GRABBER * 2.0)
		if mark[2]: # a hit
			draw_line(Vector2(x, size.y - 10), Vector2(x, size.y), Color(1.0, 0.25, 0.2), 3.0)
			continue
		draw_line(Vector2(x, size.y - 8), Vector2(x, size.y), Color(1, 1, 1, 0.6), 1.0)
		draw_string(font, Vector2(x + 2, 11 + row * 11), mark[1], HORIZONTAL_ALIGNMENT_LEFT, -1, 10, Color(1, 1, 1, 0.75))
		row = 1 - row
