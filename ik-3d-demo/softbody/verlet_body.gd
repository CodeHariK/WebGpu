## Points + rules: the whole engine behind ropes, hair, cloth and ragdolls (Verlet / position-based
## dynamics). So far: distance rules ("sticks", stiff or slack), pins, mass and a floor.
##
## Every step():
##   1. Move   — each free point keeps going the way it went last step (position − previous),
##               a little slower (damping), and gravity pulls it down. No velocity is stored:
##               where it was last step IS its velocity. That's Verlet.
##   2. Fix    — run the rules a few times (iterations). A stick says "these two points are this
##               far apart": if they're too far, pull both in; too close, push both out. A pinned
##               point never moves, so its partner takes the whole fix.
##   3. Floor  — any point below the floor goes back on top of it (and slides a bit slower).
##
## Fixing one stick can stretch the next, so one pass isn't enough: more iterations = stiffer
## (a chain), fewer = stretchier (a bungee). That's the only "stiffness" knob a rope needs.
##
## Mass: when a stick fixes two points, each moves in proportion to 1 / mass, so a heavy weight on a
## light chain barely moves and the chain does the bending. A pinned point acts as infinitely heavy.
## Slack sticks only pull (a chain or rope can't push): they are fixed only when too long.
class_name VerletBody
extends RefCounted

var points := PackedVector3Array() ## where each point is now (world space)
var previous := PackedVector3Array() ## where it was last step (points − previous = its motion)
var pinned := PackedByteArray() ## 1 = held in place (moved only by set_point)
var mass := PackedFloat32Array() ## kg; only the ratio between two points matters
var radius := PackedFloat32Array() ## how far the point stays above the floor (its size)
var sticks: Array[Vector3i] = [] ## (a, b, slack): two point indices joined by a distance rule; slack 1 = only pulls
var rest := PackedFloat32Array() ## each stick's length

var gravity := Vector3(0, -9.8, 0) ## m/s²
var damping := 0.99 ## share of last step's motion kept: 1 = swings forever, 0.9 = moves through honey
var iterations := 8 ## rule passes per step: stiffness
var floor_height := 0.0
var floor_friction := 0.5 ## share of sideways motion lost when touching the floor


## Adds a point and returns its index.
func add_point(
		position: Vector3,
		is_pinned := false,
		point_mass := 1.0,
		point_radius := 0.0
) -> int:
	points.append(position)
	previous.append(position)
	pinned.append(1 if is_pinned else 0)
	mass.append(point_mass)
	radius.append(point_radius)
	return points.size() - 1


## Joins two points with a stick as long as they are apart right now; returns the stick's index.
## slack: the stick only pulls (rope, chain), it lets the points come closer.
func add_stick(
		a: int,
		b: int,
		slack := false
) -> int:
	sticks.append(Vector3i(a, b, 1 if slack else 0))
	rest.append(points[a].distance_to(points[b]))
	return sticks.size() - 1


func remove_stick(index: int) -> void:
	sticks.remove_at(index)
	rest.remove_at(index)


## Gives a point a push: it moves `velocity` (m/s) from now on (until the rules say otherwise).
func set_velocity(
		index: int,
		velocity: Vector3,
		delta: float
) -> void:
	previous[index] = points[index] - velocity * delta


func velocity(
		index: int,
		delta: float
) -> Vector3:
	return (points[index] - previous[index]) / delta


## Moves a point by hand (a pin following a hand, the mouse). Its previous position is where it
## was, so when it is unpinned it flies off with the speed it was moved at.
func set_point(
		index: int,
		position: Vector3
) -> void:
	previous[index] = points[index]
	points[index] = position


## How far the sticks are from their rest length overall: 0 = perfect, 0.1 = 10 % longer.
func stretch() -> float:
	var now := 0.0
	var total := 0.0
	for s in sticks.size():
		now += points[sticks[s].x].distance_to(points[sticks[s].y])
		total += rest[s]
	return now / total - 1.0 if total > 0.0 else 0.0


func step(delta: float) -> void:
	_move(delta)
	for pass_index in iterations:
		_fix_sticks()
	_fix_floor()


# 1. Verlet: keep going (damped) and fall.
func _move(delta: float) -> void:
	var fall := gravity * delta * delta
	for i in points.size():
		if pinned[i] == 1:
			continue
		var now := points[i]
		points[i] = now + (now - previous[i]) * damping + fall
		previous[i] = now


# 2. Every stick back to its length; pinned points don't move, so the other end takes the fix.
func _fix_sticks() -> void:
	for s in sticks.size():
		var a := sticks[s].x
		var b := sticks[s].y
		var weight_a := 0.0 if pinned[a] == 1 else 1.0 / mass[a] # light points move more
		var weight_b := 0.0 if pinned[b] == 1 else 1.0 / mass[b]
		var total := weight_a + weight_b
		if total == 0.0:
			continue
		var along := points[b] - points[a]
		var length := along.length()
		if length < 1e-6 or (sticks[s].z == 1 and length <= rest[s]): # slack and not taut: no pull
			continue
		var fix := along * ((length - rest[s]) / length) # how far to close the gap, a → b
		points[a] += fix * (weight_a / total)
		points[b] -= fix * (weight_b / total)


# 3. Nothing goes through the floor (a point keeps its radius above it); touching it slows the slide.
func _fix_floor() -> void:
	for i in points.size():
		var lowest := floor_height + radius[i]
		if pinned[i] == 1 or points[i].y >= lowest:
			continue
		points[i].y = lowest
		var slide := points[i] - previous[i]
		previous[i].x = points[i].x - slide.x * (1.0 - floor_friction)
		previous[i].z = points[i].z - slide.z * (1.0 - floor_friction)
