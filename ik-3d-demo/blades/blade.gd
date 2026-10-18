## One blade on its chain: where it is, which move and phase it is in, and the path it is playing.
##
## A phase plays a path — one or more cubic Beziers, 4 control points each — over a fixed time
## with an easing curve (start / start_segments / start_even). Each frame the demo advances `time`, reads
## eased_position() and puts the tip on the path there. When the time is up the blade's move
## decides what comes next (BladeMove.next_phase).
class_name Blade
extends RefCounted

const IDLE := 0
const OUT := 1 ## hand → target (wind-up + strike), or → the start of a sweep / spin / slash
const HOLD := 2 ## stuck in the target
const SWEEP := 3
const SPIN := 4
const YANK := 5 ## enemy (and blades) along a lifted arc
const BACK := 6 ## → hand
const SLICE := 7 ## one slash of a slice combo
const WHIRL := 8 ## swinging a hooked enemy round the player
const LASSO := 9 ## one chain swinging a hooked enemy in a ring above the player's head
const PHASE_NAMES := ["idle", "out", "hold", "sweep", "spin", "yank", "back", "slice", "whirl", "lasso"]

var side := 1.0 ## +1 right hand, −1 left hand
var phase := IDLE
var move := "" ## the key of the move playing (BladesDemo.moves)
var delay := 0.0 ## waits this long before the phase starts playing
var time := 0.0
var duration := 0.0
var segment_times := PackedFloat32Array() ## one time per path segment (or one for the whole path)
var segment_easings := [] ## an easing name per segment
var eased := 0.0 ## where along the path the tip is (0..1, over all segments)
var path := PackedVector3Array() ## 4 control points per Bezier segment
var base_path := PackedVector3Array() ## the path as its move built it (before the curve editor's offsets)
var swing_end := 0.0 ## OUT: where along the path (0..1) the wind-up ends (0: this path has none)
var tip := Vector3.ZERO
var previous_tip := Vector3.ZERO
var enemy := -1 ## the hooked enemy (pull, rush, whirl)
var hit: Array[int] = [] ## enemies already hit (this move, or this spin turn)
var turn := 0 ## spin: the turn it is on
var spin_right := Vector3.RIGHT ## spin: the circle's frame, fixed when the spin starts
var spin_forward := Vector3.FORWARD
var slice_angles: Array[float] = [] ## slice: this blade's slash planes still to do
var chain: LimbChain
var chain_on_path := false ## last frame the chain lay on the path (thrown / reeling); else taut
var snap_from := PackedVector3Array() ## the chain's shape when it switched (it blends from this)
var snap_time := 99.0 ## seconds since it switched
var sway := ChainSway.new() ## the chain's weight: it follows its shape on springs
var mesh: MeshInstance3D

static var shaper := Callable() ## BladesDemo.shape_path: the curve editor's offsets, added to every path a blade starts
static var timing: PhaseTiming ## the timing editor's edits (time scale, speed curve), applied to every phase a blade starts

const LENGTH_SAMPLES := 16 ## per segment, for start_even's length table

var _lengths := PackedFloat32Array() ## start_even: arc length from the path start at each sample (empty: segments share time equally)
var _curve := Vector4.ZERO ## the timing editor's speed curve for this phase
var _has_curve := false


## Play `path` over `duration` with one easing for the whole of it.
func start(
		new_phase: int,
		new_path: PackedVector3Array,
		new_duration: float,
		easing: String
) -> void:
	start_segments(new_phase, new_path, PackedFloat32Array([new_duration]), [easing])


## Play `path` with its own time and easing per segment (wind-up slow, strike fast …).
func start_segments(
		new_phase: int,
		new_path: PackedVector3Array,
		times: PackedFloat32Array,
		easings: Array
) -> void:
	phase = new_phase
	base_path = new_path
	path = shaper.call(self, new_path) if shaper.is_valid() else new_path
	swing_end = 0.0
	_lengths.clear()
	segment_times = times
	segment_easings = easings
	duration = 0.0
	for t in times:
		duration += t
	time = 0.0
	eased = 0.0
	turn = 0
	_has_curve = false
	if timing != null:
		_retime(PhaseTiming.key_of(move, phase))


# The timing editor's edits for this phase (key "move/phase"): its time scaled, and its own speed
# curve over the whole path (measured along its length) in place of the move's easings.
func _retime(key: String) -> void:
	var scale := timing.scale_for(key, segment_easings[0], duration)
	if scale != 1.0:
		for i in segment_times.size():
			segment_times[i] *= scale
		duration *= scale
	if timing.curves.has(key):
		_curve = timing.curves[key]
		_has_curve = true
		segment_times = PackedFloat32Array([duration])
		segment_easings = ["linear"]
		_measure()


## Play `path` over `duration` with one easing measured along its LENGTH: the tip's speed follows
## the easing whatever the segments' sizes, so a path of several segments plays as one curve
## (no speed jump where they join).
func start_even(
		new_phase: int,
		new_path: PackedVector3Array,
		new_duration: float,
		easing: String
) -> void:
	start(new_phase, new_path, new_duration, easing)
	_measure()


# start_even's length table: arc length from the path start at LENGTH_SAMPLES points per segment.
func _measure() -> void:
	_lengths.clear()
	_lengths.append(0.0)
	var last := path[0]
	for k in range(0, path.size() - 3, 4):
		for s in LENGTH_SAMPLES:
			var p := Bezier.point(path[k], path[k + 1], path[k + 2], path[k + 3], (s + 1.0) / LENGTH_SAMPLES)
			_lengths.append(_lengths[-1] + last.distance_to(p))
			last = p


## Stay where the tip is for `hold_time`.
func hold(hold_time: float) -> void:
	start(HOLD, PackedVector3Array([tip, tip, tip, tip]), hold_time, "linear")


## Share of the phase's time used up (0..1).
func progress() -> float:
	return clampf(time / duration, 0.0, 1.0) if duration > 0.0 else 1.0


## Where along the path (0..1) the blade is now: one easing over the whole path, or per segment —
## each segment then gets its own share of the time and its own speed curve.
func eased_position() -> float:
	if segment_times.size() == 1:
		var along := PhaseTiming.curve_at(progress(), _curve) if _has_curve else eased_time(progress(), segment_easings[0])
		return along if _lengths.is_empty() else _at_length(along * _lengths[-1])
	var start_time := 0.0
	for i in segment_times.size():
		if time < start_time + segment_times[i] or i == segment_times.size() - 1:
			var local := clampf((time - start_time) / segment_times[i], 0.0, 1.0)
			return (i + eased_time(local, segment_easings[i])) / segment_times.size()
		start_time += segment_times[i]
	return 1.0


# The path position (0..1) at `length` along the path (start_even's table, linear in between).
func _at_length(length: float) -> float:
	var low := 0
	var high := _lengths.size() - 1
	while high - low > 1: # binary search: _lengths[low] <= length < _lengths[high]
		var middle := (low + high) / 2
		if _lengths[middle] <= length:
			low = middle
		else:
			high = middle
	var span := _lengths[high] - _lengths[low]
	var fraction := (length - _lengths[low]) / span if span > 1e-6 else 0.0
	return clampf((low + fraction) / (_lengths.size() - 1), 0.0, 1.0)


## The speed curves: how a phase's time t (0..1) maps to distance along its path.
static func eased_time(
		t: float,
		easing: String
) -> float:
	match easing:
		"out": return 1.0 - pow(1.0 - t, 3.0) # fast start, soft landing: the throw
		"in": return t * t * t # slow start, violent end: the yank
		"accelerate": return t * t # faster and faster: the whirl
		"smooth": return t * t * (3.0 - 2.0 * t)
		"swing": # wind-up + strike as one motion: slow back, fast through, soft landing
			var u := pow(t, 1.6)
			return u * u * (3.0 - 2.0 * u)
	return t
