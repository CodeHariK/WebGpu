## Timing edits per move and phase ("pull/yank"), made on the tuning panel's timing editor:
##   scales  the phase takes this × its time (slower / quicker);
##   curves  its speed curve (time → distance along the path) replaced by a cubic Bezier through
##           (0, 0) and (1, 1) with two handles (x1, y1, x2, y2) — like CSS's cubic-bezier(). A
##           path of several pieces then plays as one curve, measured along its length.
## Blade.start_segments asks timing_for() every time a phase starts; a phase with no edit plays as
## its move made it. `seen` remembers each phase a move started (its easing name and time) so the
## editor can list them and show the curve it has now.
class_name PhaseTiming
extends RefCounted

const LINEAR := Vector4(1.0 / 3.0, 1.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0)
const PRESETS := { ## the handles closest to each named easing (where an edit starts from)
	"linear": LINEAR,
	"out": Vector4(0.33, 1.0, 0.68, 1.0),
	"in": Vector4(0.32, 0.0, 0.67, 0.0),
	"accelerate": Vector4(0.11, 0.0, 0.5, 0.0),
	"smooth": Vector4(0.65, 0.0, 0.35, 1.0),
	"swing": Vector4(0.6, 0.0, 0.3, 1.0),
}

var curves := {} ## "move/phase" → Vector4 handles
var scales := {} ## "move/phase" → float
var seen := {} ## "move/phase" → {"easing": name, "time": seconds as the move made it}


static func key_of(
		move: String,
		phase: int
) -> String:
	return "%s/%s" % [move, Blade.PHASE_NAMES[phase]]


## A phase starts: note it (for the editor); its time scale (1: none).
func scale_for(
		key: String,
		easing: String,
		time: float
) -> float:
	if not seen.has(key):
		seen[key] = {"easing": easing, "time": time}
	return float(scales.get(key, 1.0))


## Forget the edits of one move (all its phases).
func clear(move: String) -> void:
	for table: Dictionary in [curves, scales]:
		for key: String in table.keys():
			if key.begins_with(move + "/"):
				table.erase(key)


## The phases `move` has started (as "move/phase" keys), in the order first seen.
func phases_of(move: String) -> Array[String]:
	var out: Array[String] = []
	for key: String in seen:
		if key.begins_with(move + "/"):
			out.append(key)
	return out


## The cubic-bezier speed curve at time t (0..1): distance along the path (0..1; may overshoot a
## little if a handle is above 1 or below 0).
static func curve_at(
		t: float,
		c: Vector4
) -> float:
	if t <= 0.0:
		return 0.0
	if t >= 1.0:
		return 1.0
	var s := t # solve x(s) = t: a few Newton steps, bisection if the slope is flat
	for i in 6:
		var dx := _bezier_slope(s, c.x, c.z)
		if absf(dx) < 1e-5:
			break
		s = clampf(s - (_bezier(s, c.x, c.z) - t) / dx, 0.0, 1.0)
	if absf(_bezier(s, c.x, c.z) - t) > 1e-4:
		var low := 0.0
		var high := 1.0
		for i in 30:
			s = (low + high) * 0.5
			if _bezier(s, c.x, c.z) < t:
				low = s
			else:
				high = s
	return _bezier(s, c.y, c.w)


static func _bezier(
		s: float,
		a: float,
		b: float
) -> float:
	var r := 1.0 - s
	return 3.0 * r * r * s * a + 3.0 * r * s * s * b + s * s * s


static func _bezier_slope(
		s: float,
		a: float,
		b: float
) -> float:
	var r := 1.0 - s
	return 3.0 * r * r * a + 6.0 * r * s * (b - a) + 3.0 * s * s * (1.0 - b)
