## A footfall chart, like the gait diagrams in biology papers: one row per leg (L1 … L4, R1 … R4),
## time running left to right over the last SECONDS. A bar while the foot is on the ground, a gap
## while it's in the air. Orange = left legs, grey = right. The diagonal stripes of the spider
## gait (and how they ripple) show up at a glance.
class_name GaitChart
extends Control

const SECONDS := 3.0
const SAMPLES := 180 ## one per physics frame at 60 Hz
const ROW := 13.0
const WIDTH := 360.0
const LABEL := 26.0

var spider: GaitSpider

var _history: Array[PackedByteArray] = []
var _head := 0


func _ready() -> void:
	custom_minimum_size = Vector2(LABEL + WIDTH + 8.0, ROW * 8.0 + 24.0)
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func _physics_process(_delta: float) -> void:
	if spider == null or spider.legs.is_empty():
		return
	if _history.size() != spider.legs.size():
		_history.clear()
		for leg in spider.legs:
			var row := PackedByteArray()
			row.resize(SAMPLES)
			row.fill(1)
			_history.append(row)
	_head = (_head + 1) % SAMPLES
	for i in spider.legs.size():
		_history[i][_head] = 0 if spider.legs[i].swinging else 1
	queue_redraw()


func _draw() -> void:
	if spider == null or _history.is_empty():
		return
	var rows := spider.legs.size()
	draw_rect(Rect2(Vector2.ZERO, Vector2(LABEL + WIDTH + 8.0, ROW * rows + 22.0)), Color(0, 0, 0, 0.45))
	var font := ThemeDB.fallback_font
	draw_string(font, Vector2(4, 14), "footfalls (bar = on the ground), last %.0f s" % SECONDS, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(0.8, 0.8, 0.8))
	var step := WIDTH / SAMPLES
	for i in rows:
		var leg := spider.legs[i]
		var y := 20.0 + i * ROW
		var color := Color(1.0, 0.55, 0.2) if leg.side < 0 else Color(0.75, 0.75, 0.8)
		draw_string(font, Vector2(4, y + ROW - 3.0), leg.name, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, color)
		for s in SAMPLES:
			var index := (_head + 1 + s) % SAMPLES # oldest first
			if _history[i][index] == 1:
				draw_rect(Rect2(LABEL + s * step, y + 2.0, step + 0.5, ROW - 4.0), color)
