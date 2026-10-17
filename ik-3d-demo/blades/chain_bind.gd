## One chain of a Chain Bind: it bursts out of the ground and runs along a path — any list of
## points made smooth (Bezier.from_points) — then holds, then slides back into the ground the way
## it came. The demo makes the points: an anchor under the ground, a point just above it, then a
## helix round the enemy, so the chain rises and coils round it.
##
## The visible chain is always the path from its start up to how far it has grown, so it never
## leaves its path. Growth is measured along the path's LENGTH (not its t), so it moves at an
## even speed whatever the point spacing.
class_name ChainBind
extends RefCounted

const GROW := 0
const HOLD := 1
const SINK := 2
const DONE := 3

var points := PackedVector3Array() ## the polyline it was made from (the debug view draws it)
var path := PackedVector3Array() ## Bezier segments through those points
var phase := GROW
var delay := 0.0
var grow_time := 0.45
var hold_time := 1.4
var sink_time := 0.35
var grown := 0.0 ## share of the path's length the chain covers (0..1)
var chain: LimbChain

var _time := 0.0
var _table := PackedFloat32Array() ## path length share at evenly spaced t, for constant speed


func _init(
		route: PackedVector3Array,
		links: int,
		color: Color
) -> void:
	points = route
	path = Bezier.from_points(route)
	_measure()
	var lengths := PackedFloat32Array()
	lengths.resize(links - 1)
	lengths.fill(0.1)
	chain = LimbChain.new(lengths, route[0], LimbChain.Solver.FK)
	chain.style = LimbChain.Style.BLOCK
	chain.radius = 0.035
	chain.gap = 0.012
	chain.color = color
	chain.points = _link_points(links)


## A helix round `center`: anchor below the ground → up out of it → `turns` turns at `radius`,
## from height h0 to h1 (above center's foot), going round in `direction` (+1 / −1).
static func coil_points(
		anchor: Vector3,
		center: Vector3,
		radius: float,
		start_angle: float,
		turns: float,
		h0: float,
		h1: float,
		direction: float,
		per_turn := 8
) -> PackedVector3Array:
	var route := PackedVector3Array([anchor + Vector3.DOWN * 0.4, anchor + Vector3.UP * 0.7])
	var count := int(turns * per_turn)
	for k in count + 1:
		var angle := start_angle + direction * TAU * k / per_turn
		var height := lerpf(h0, h1, float(k) / count)
		route.append(center + Vector3(cos(angle), 0.0, sin(angle)) * radius + Vector3.UP * height)
	return route


func update(delta: float) -> void:
	if phase == DONE:
		return
	if delay > 0.0:
		delay -= delta
		return
	_time += delta
	match phase:
		GROW:
			var t := minf(_time / grow_time, 1.0)
			grown = 1.0 - (1.0 - t) * (1.0 - t) # fast out of the ground, settles as it closes
			if t >= 1.0:
				_next(HOLD)
		HOLD:
			grown = 1.0
			if _time >= hold_time:
				_next(SINK)
		SINK:
			var t := minf(_time / sink_time, 1.0)
			grown = 1.0 - t * t # lets go slowly, then whips back into the ground
			if t >= 1.0:
				_next(DONE)
	chain.points = _link_points(chain.points.size())


func tip() -> Vector3:
	return Bezier.path_point(path, _t_at(grown))


func _next(next_phase: int) -> void:
	phase = next_phase
	_time = 0.0


func _link_points(count: int) -> PackedVector3Array:
	if grown < 1e-3:
		var hidden := PackedVector3Array()
		hidden.resize(count)
		hidden.fill(path[0]) # all links at the anchor, under the ground
		return hidden
	return Bezier.path_even_points(path, 0.0, _t_at(grown), count)


# Path length share at 200 evenly spaced t: walk the path once, add up the steps.
func _measure() -> void:
	var samples := 200
	_table.resize(samples + 1)
	var total := 0.0
	var last := path[0]
	_table[0] = 0.0
	for i in range(1, samples + 1):
		var p := Bezier.path_point(path, float(i) / samples)
		total += p.distance_to(last)
		_table[i] = total
		last = p
	for i in samples + 1:
		_table[i] /= maxf(total, 1e-6)


# The path's t at which `share` of its length is covered (inverse of the table).
func _t_at(share: float) -> float:
	var samples := _table.size() - 1
	var i := _table.bsearch(clampf(share, 0.0, 1.0))
	if i <= 0:
		return 0.0
	if i > samples:
		return 1.0
	var span := _table[i] - _table[i - 1]
	var f := (share - _table[i - 1]) / span if span > 1e-6 else 0.0
	return (i - 1 + f) / samples
