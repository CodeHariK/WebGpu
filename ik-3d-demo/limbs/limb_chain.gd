## A limb without Skeleton3D: just joint positions in world space, solved on the spot.
##
##   points[0] is the root (hip/shoulder), points[n] the tip (foot/hand); segment i runs from
##   points[i] to points[i + 1] and is lengths[i] long. Set root, target and pole, call solve(),
##   and the result is there immediately — no deferred skeleton update, no NodePaths, no nodes.
##
## Solvers:
##   TWO_BONE  exact, analytic (law of cosines). Two segments. The knee bends toward `pole`.
##   THREE_BONE  hip → knee → ankle → foot. The last segment (a spider's tarsus, a stilt foot) hangs
##             along `tip_direction`; the first two solve exactly (two-bone) to put the ankle
##             right above the foot. Stable, and the foot plants at a fixed angle — unless the
##             target is too high or far for that, then the foot swings out just enough to reach.
##   AIM       one segment pointing at the target, stretching up to `max_stretch` × its length
##             (t3ssel8r robot legs).
##   FABRIK    any number of segments (tails, tentacles, necks, stalks). Iterative: forward and
##             backward passes until the tip is within `tolerance` or `iterations` run out. The
##             joints are nudged toward `pole` first, so it bends the same way every frame.
##   FK        no solving: the owner writes `points` itself (a dancing stem posed by angles);
##             the chain is just something to draw.
##   ROPE      a damped Verlet rope: every free point keeps its momentum (× rope_damping), falls
##             with rope_gravity, then `iterations` passes pull each segment back to its length.
##             The root is pinned; the tip is pinned to `target` too when pin_tip (a hose between
##             two hands) or hangs free (a tail). Set time_step to the frame's delta first.
##
## segment_basis() gives each segment a twist-free frame (+Y along the segment, +X the bend
## plane's normal), so a renderer can draw a mesh per segment without bones.
class_name LimbChain
extends RefCounted

enum Solver { TWO_BONE, THREE_BONE, AIM, FABRIK, FK, ROPE }
enum Style { ROUND, BLOCK } ## how LimbRenderer draws it: cylinders + joint balls, or square blocks


var solver := Solver.TWO_BONE
var lengths := PackedFloat32Array()
var points := PackedVector3Array()
var root := Vector3.ZERO
var target := Vector3.ZERO
var pole := Vector3.UP ## world point the joints bend toward
var max_stretch := 1.0 ## AIM only
var tip_direction := Vector3.DOWN ## THREE_BONE: world direction of the last segment (ankle → foot)
var iterations := 8 ## FABRIK and ROPE
var tolerance := 0.002 ## FABRIK only, metres
var pin_tip := true ## ROPE: tip held at `target` (false = hangs free)
var rope_gravity := Vector3(0, -9.8, 0) ## ROPE, m/s²
var rope_damping := 0.96 ## ROPE: share of last step's motion kept (1 = never settles)
var time_step := 1.0 / 60.0 ## ROPE: seconds since the last solve()
# Look (read by LimbRenderer).
var style := Style.ROUND
var radius := 0.03 ## segment radius (ROUND) or half width (BLOCK), at the root
var taper := 1.0 ## radius at the tip ÷ radius at the root (segments shrink linearly)
var joint_radius := -1.0 ## ROUND joint balls; < 0 = radius × 1.35 (= radius gives a capsule look)
var gap := 0.0 ## BLOCK: each block is this much shorter at both ends, so the pieces float apart
var color := Color.WHITE

var _reach := 0.0
var _previous := PackedVector3Array() ## ROPE: last step's points (velocity = points − previous)


## A chain of `segment_lengths` hanging from `start`, straight down.
func _init(segment_lengths: PackedFloat32Array, start := Vector3.ZERO, chain_solver := Solver.TWO_BONE) -> void:
	lengths = segment_lengths
	solver = chain_solver
	root = start
	points.resize(lengths.size() + 1)
	points[0] = start
	_reach = 0.0
	for i in lengths.size():
		_reach += lengths[i]
		points[i + 1] = start + Vector3.DOWN * _reach
	target = points[-1]


func segment_count() -> int:
	return lengths.size()


func tip() -> Vector3:
	return points[-1]


## Total length when straight.
func reach() -> float:
	return _reach


func solve() -> void:
	match solver:
		Solver.TWO_BONE: _solve_two_bone(target)
		Solver.THREE_BONE: _solve_three_bone()
		Solver.AIM: _solve_aim()
		Solver.FABRIK: _solve_fabrik()
		Solver.FK: pass # points are set by the owner
		Solver.ROPE: _solve_rope()


## How far the tip ended up from the target (0 = reached it).
func error() -> float:
	return points[-1].distance_to(target)


## Frame for segment i: +Y along the segment (unit length), +X the bend plane's normal.
func segment_basis(i: int) -> Basis:
	var along := points[i + 1] - points[i]
	var length := along.length()
	var y := along / length if length > 1e-6 else Vector3.DOWN
	var normal := _bend_normal()
	var x := normal - y * normal.dot(y) # the bend normal, made square to this segment
	if x.length_squared() < 1e-8:
		x = y.cross(Vector3.FORWARD if absf(y.z) < 0.9 else Vector3.RIGHT)
	x = x.normalized()
	return Basis(x, y, x.cross(y))


## Radius of segment i (tapering from `radius` at the root toward radius × taper at the tip).
func segment_radius(i: int) -> float:
	return radius * lerpf(1.0, taper, float(i) / segment_count())


## Radius of the ball at joint k (ROUND): joint_radius (or radius × 1.35), tapered like the
## segment that starts there (the tip uses the last segment's).
func joint_ball_radius(k: int) -> float:
	var base := joint_radius if joint_radius >= 0.0 else radius * 1.35
	return base * lerpf(1.0, taper, float(mini(k, segment_count() - 1)) / segment_count())


## Current length of segment i (differs from lengths[i] only for a stretching AIM).
func segment_length(i: int) -> float:
	return points[i].distance_to(points[i + 1])


# --- solvers ------------------------------------------------------------------------------------

# Exact two-bone solve of points[0..2] toward `goal` (the knee bends toward the pole).
func _solve_two_bone(goal: Vector3) -> void:
	var a := lengths[0]
	var b := lengths[1]
	var to_target := goal - root
	var distance := clampf(to_target.length(), absf(a - b) + 1e-4, a + b - 1e-4)
	var direction := _direction(to_target)
	var bend := _perpendicular_toward_pole(direction)
	var cos_root := (a * a + distance * distance - b * b) / (2.0 * a * distance) # law of cosines
	var sin_root := sqrt(maxf(0.0, 1.0 - cos_root * cos_root))
	points[0] = root
	points[1] = root + direction * (a * cos_root) + bend * (a * sin_root)
	points[2] = root + direction * distance


# The foot segment hangs along tip_direction from the ankle; two-bone puts the ankle above the foot.
# If that ankle is out of the first two bones' reach (a foot lifted high or far out), the foot
# swings toward the straight hip → target line — only as far as needed (bisection) — so the leg
# still reaches its target whenever its full length allows.
func _solve_three_bone() -> void:
	var foot := _direction(tip_direction)
	var reach_two := (lengths[0] + lengths[1]) * 0.999
	if (target - foot * lengths[2] - root).length() > reach_two:
		var straight := _direction(target - root)
		var low := 0.0
		var high := 1.0
		for step in 10:
			var middle := (low + high) * 0.5
			if (target - foot.slerp(straight, middle) * lengths[2] - root).length() > reach_two:
				low = middle
			else:
				high = middle
		foot = foot.slerp(straight, high)
	_solve_two_bone(target - foot * lengths[2])
	points[3] = points[2] + foot * lengths[2]


func _solve_aim() -> void:
	var to_target := target - root
	points[0] = root
	points[1] = root + _direction(to_target) * minf(to_target.length(), lengths[0] * max_stretch)


func _solve_fabrik() -> void:
	var count := points.size()
	var to_target := target - root
	if to_target.length() >= _reach: # out of reach: just point straight at it
		var direction := _direction(to_target)
		points[0] = root
		for i in lengths.size():
			points[i + 1] = points[i] + direction * lengths[i]
		return
	_nudge_toward_pole()
	for pass_index in iterations:
		points[count - 1] = target # backward: from the tip
		for i in range(count - 2, -1, -1):
			points[i] = points[i + 1] + _direction(points[i] - points[i + 1]) * lengths[i]
		points[0] = root # forward: from the root
		for i in range(1, count):
			points[i] = points[i - 1] + _direction(points[i] - points[i - 1]) * lengths[i - 1]
		if points[count - 1].distance_squared_to(target) < tolerance * tolerance:
			break


func _solve_rope() -> void:
	var count := points.size()
	if _previous.size() != count:
		_previous = points.duplicate()
	var last_free := count - 1 if pin_tip else count # exclusive end of the free points
	var fall := rope_gravity * time_step * time_step
	for i in range(1, last_free): # Verlet: keep moving, damped, and fall
		var point := points[i]
		points[i] = point + (point - _previous[i]) * rope_damping + fall
		_previous[i] = point
	points[0] = root
	_previous[0] = root
	if pin_tip:
		points[count - 1] = target
		_previous[count - 1] = target
	for pass_index in iterations: # pull every segment back to its length
		for i in count - 1:
			var a_pinned := i == 0
			var b_pinned := pin_tip and i + 1 == count - 1
			var along := points[i + 1] - points[i]
			var length := along.length()
			if length < 1e-6 or (a_pinned and b_pinned):
				continue
			var fix := along * ((length - lengths[i]) / length)
			if a_pinned:
				points[i + 1] -= fix
			elif b_pinned:
				points[i] += fix
			else:
				points[i] += fix * 0.5
				points[i + 1] -= fix * 0.5


# Push interior joints a little toward the pole so FABRIK keeps bending the same way.
func _nudge_toward_pole() -> void:
	var direction := _direction(target - root)
	var bend := _perpendicular_toward_pole(direction)
	for i in range(1, points.size() - 1):
		points[i] += bend * lengths[i - 1] * 0.25


# --- helpers ------------------------------------------------------------------------------------

func _perpendicular_toward_pole(direction: Vector3) -> Vector3:
	var to_pole := pole - root
	var bend := to_pole - direction * to_pole.dot(direction)
	if bend.length_squared() < 1e-8: # pole on the line: pick any perpendicular
		bend = direction.cross(Vector3.RIGHT if absf(direction.x) < 0.9 else Vector3.FORWARD)
	return bend.normalized()


func _bend_normal() -> Vector3:
	var normal := (target - root).cross(pole - root)
	return normal.normalized() if normal.length_squared() > 1e-8 else Vector3.RIGHT


static func _direction(v: Vector3) -> Vector3:
	return v / v.length() if v.length_squared() > 1e-12 else Vector3.DOWN
