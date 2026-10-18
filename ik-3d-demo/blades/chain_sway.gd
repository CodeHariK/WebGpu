## Weight for a blade's chain: the demo works out the shape the chain should have (BladeChain: on
## the path, or taut), and this makes the links FOLLOW that shape on springs instead of sitting on
## it — so the chain sags under its own weight, lags a little behind a fast hand, and bounces
## when it snaps tight. No rope physics: each link is a damped spring to its place on the shape.
##
## How much it does depends on the chain's state (BladesDemo picks it each frame):
##   sag   metres the middle hangs below the shape (more when slack, a little under load);
##   hz    how stiff the springs are (soft: slow swing; stiff: a quick buzz when it snaps tight);
##   sway  0..1: how much of the springs' lag shows (low while it lies on a flying blade's path,
##         so the throw still draws its curve).
## The two ends never move: the hand and the blade stay where they are.
class_name ChainSway
extends RefCounted

const MAX_STEP := 0.25 ## the springs' sub-step: at most this × 1/ω (keeps a stiff spring stable)
const TELEPORT := 2.0 ## m: the hand jumped this far in a frame (a reset): start again on the shape

var _position := PackedVector3Array() ## each link where its spring has it
var _velocity := PackedVector3Array()


## The chain's links for this frame: `shape` sagged by `sag`, followed on springs.
func apply(
		shape: PackedVector3Array,
		sag: float,
		hz: float,
		damping: float,
		sway: float,
		delta: float
) -> PackedVector3Array:
	var target := sagged(shape, sag)
	var count := target.size()
	if _position.size() != count or _position[0].distance_to(target[0]) > TELEPORT:
		_position = target.duplicate()
		_velocity.resize(count)
		_velocity.fill(Vector3.ZERO)
	_spring(target, TAU * hz, damping, delta)
	var out := target.duplicate()
	var last := maxf(count - 1, 1)
	for k in range(1, count - 1):
		var pin := sqrt(sin(PI * k / last)) # 0 at the hand and the blade, 1 in the middle
		out[k] = target[k].lerp(_position[k], pin * sway)
	return out


## `shape` with its middle lowered by `sag` (a parabola, 0 at the ends): less when the chain hangs
## near-vertical (a vertical chain doesn't sag sideways).
static func sagged(
		shape: PackedVector3Array,
		sag: float
) -> PackedVector3Array:
	var out := shape.duplicate()
	var count := shape.size()
	if sag <= 0.0 or count < 3:
		return out
	var span := shape[-1] - shape[0]
	var level := 1.0 - absf(span.normalized().y) if span.length() > 1e-3 else 0.0
	var last := float(count - 1)
	for k in range(1, count - 1):
		var u := k / last
		out[k] += Vector3.DOWN * sag * level * 4.0 * u * (1.0 - u)
	return out


## Total length along the links.
static func length_of(points: PackedVector3Array) -> float:
	var total := 0.0
	for k in range(1, points.size()):
		total += points[k].distance_to(points[k - 1])
	return total


# Every link's spring toward its place: x'' = ω²(target − x) − 2ζω x', in sub-steps small enough
# for a stiff spring to stay stable.
func _spring(
		target: PackedVector3Array,
		omega: float,
		damping: float,
		delta: float
) -> void:
	if delta <= 0.0:
		return
	var steps := ceili(delta * omega / MAX_STEP)
	var h := delta / steps
	for s in steps:
		for k in target.size():
			var pull := (target[k] - _position[k]) * omega * omega - _velocity[k] * 2.0 * damping * omega
			_velocity[k] += pull * h
			_position[k] += _velocity[k] * h
