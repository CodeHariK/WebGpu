## A footfall chart for the crawler: one row per limb (LH RH LF RF), the last few seconds running
## left to right. A solid bar while the hand/foot is planted, a gap while it steps, a dark red
## bar while the crawler is in the air. With the diagonal-pair gait, LH lines up with RF and
## RH with LF, and the two pairs alternate (gaitspider/gait_chart.gd, for four limbs).
class_name CrawlerChart
extends Control

const SECONDS := 3.0
const SAMPLES := 180 ## one per physics frame at 60 Hz
const ROW := 14.0
const WIDTH := 300.0
const LABEL := 26.0
const NAMES := ["LH", "RH", "LF", "RF"]
enum { PLANTED, STEPPING, AIR }

var crawler: Crawler

var _history: Array[PackedByteArray] = []
var _head := 0


func _ready() -> void:
	custom_minimum_size = Vector2(LABEL + WIDTH + 8.0, ROW * 4.0 + 26.0)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	for limb in CrawlerLayout.COUNT:
		var row := PackedByteArray()
		row.resize(SAMPLES)
		row.fill(PLANTED)
		_history.append(row)


func _physics_process(_delta: float) -> void:
	if crawler == null or not visible:
		return
	_head = (_head + 1) % SAMPLES
	for limb in CrawlerLayout.COUNT:
		var step := crawler.gait.steps[limb]
		_history[limb][_head] = AIR if step.airborne else (STEPPING if step.stepping else PLANTED)
	queue_redraw()


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, custom_minimum_size), Color(0, 0, 0, 0.5))
	var font := ThemeDB.fallback_font
	draw_string(font, Vector2(4, 14), "footfalls (bar = planted, gap = stepping, red = air)", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(0.85, 0.85, 0.85))
	var step := WIDTH / SAMPLES
	for limb in CrawlerLayout.COUNT:
		var y := 22.0 + limb * ROW
		var color := Color(1.0, 0.55, 0.2) if CrawlerLayout.group_of(limb) == 0 else Color(0.6, 0.8, 1.0)
		draw_string(font, Vector2(4, y + ROW - 3.0), NAMES[limb], HORIZONTAL_ALIGNMENT_LEFT, -1, 11, color)
		for s in SAMPLES:
			var state := _history[limb][(_head + 1 + s) % SAMPLES]
			if state == PLANTED:
				draw_rect(Rect2(LABEL + s * step, y + 2.0, step + 0.5, ROW - 4.0), color)
			elif state == AIR:
				draw_rect(Rect2(LABEL + s * step, y + 2.0, step + 0.5, ROW - 4.0), Color(0.5, 0.1, 0.1))
