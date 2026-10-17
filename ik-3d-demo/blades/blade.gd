## One blade on its chain: where it is, which move and phase it is in, and the path it is playing.
##
## A phase plays a path — one or more cubic Beziers, 4 control points each — over a fixed time
## with an easing curve (start / start_segments). Each frame the demo advances `time`, reads
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
const PHASE_NAMES := ["idle", "out", "hold", "sweep", "spin", "yank", "back", "slice", "whirl"]

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
var tip := Vector3.ZERO
var previous_tip := Vector3.ZERO
var enemy := -1 ## the hooked enemy (pull, rush, whirl)
var hit: Array[int] = [] ## enemies already hit (this move, or this spin turn)
var turn := 0 ## spin: the turn it is on
var spin_right := Vector3.RIGHT ## spin: the circle's frame, fixed when the spin starts
var spin_forward := Vector3.FORWARD
var slice_angles: Array[float] = [] ## slice: this blade's slash planes still to do
var chain: LimbChain
var mesh: MeshInstance3D


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
	path = new_path
	segment_times = times
	segment_easings = easings
	duration = 0.0
	for t in times:
		duration += t
	time = 0.0
	eased = 0.0
	turn = 0


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
		return eased_time(progress(), segment_easings[0])
	var start_time := 0.0
	for i in segment_times.size():
		if time < start_time + segment_times[i] or i == segment_times.size() - 1:
			var local := clampf((time - start_time) / segment_times[i], 0.0, 1.0)
			return (i + eased_time(local, segment_easings[i])) / segment_times.size()
		start_time += segment_times[i]
	return 1.0


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
	return t
