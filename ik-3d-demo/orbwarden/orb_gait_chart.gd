## Footfall chart for a Spider (like gaitspider's GaitChart, which only knows GaitSpider): one row
## per leg in layout order (L1 R1 L2 R2 L3 R3 for the warden), time running left to right over the
## last SECONDS. A bar while the foot is planted, a gap while it steps. Left legs orange, right
## legs grey. The wave gait shows up as a staircase: each row's gap a sixth of a cycle after the
## one it follows. The title gives the live cycle time and planted share.
class_name OrbGaitChart
extends Control

const SECONDS := 4.0
const SAMPLES := 240 ## one per physics frame at 60 Hz
const ROW := 14.0
const WIDTH := 380.0
const LABEL := 28.0
const LEFT := Color(1.0, 0.55, 0.2)
const RIGHT := Color(0.75, 0.75, 0.8)

var spider: Spider

var _history: Array[PackedByteArray] = []
var _head := 0


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	custom_minimum_size = Vector2(LABEL + WIDTH + 8.0, ROW * 6.0 + 26.0)


func _physics_process(_delta: float) -> void:
	if spider == null or spider.legs.is_empty() or not visible:
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
		_history[i][_head] = 0 if spider.legs[i].stepping else 1
	queue_redraw()


func _draw() -> void:
	if spider == null or _history.is_empty():
		return
	var rows := _history.size()
	custom_minimum_size.y = ROW * rows + 26.0
	draw_rect(Rect2(Vector2.ZERO, Vector2(LABEL + WIDTH + 8.0, ROW * rows + 26.0)), Color(0, 0, 0, 0.5))
	var font := ThemeDB.fallback_font
	var title := "footfalls, last %.0f s (bar = planted)" % SECONDS
	if spider.wave_gait:
		title += "   cycle %.2f s" % spider._wave_period
	draw_string(font, Vector2(4, 15), title, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color(0.85, 0.85, 0.85))
	var step := WIDTH / SAMPLES
	for i in rows:
		var definition := spider.rig.layout.legs[i]
		var y := 22.0 + i * ROW
		var color := LEFT if definition.hip.x < 0.0 else RIGHT
		draw_string(font, Vector2(4, y + ROW - 3.0), _short_name(definition.name), HORIZONTAL_ALIGNMENT_LEFT, -1, 11, color)
		var start := -1 # draw runs of planted samples as one rect each
		for s in SAMPLES + 1:
			var planted := s < SAMPLES and _history[i][(_head + 1 + s) % SAMPLES] == 1 # oldest first
			if planted and start < 0:
				start = s
			elif not planted and start >= 0:
				draw_rect(Rect2(LABEL + start * step, y + 2.0, (s - start) * step, ROW - 4.0), color)
				start = -1


# "left_2" → "L2", "right_1" → "R1"; anything else unchanged.
static func _short_name(leg_name: String) -> String:
	var parts := leg_name.split("_")
	if parts.size() == 2 and (parts[0] == "left" or parts[0] == "right"):
		return parts[0].left(1).to_upper() + parts[1]
	return leg_name
