## Stairs up the mountain: small step blocks spiralling round it, each one RISE higher than the last.
##
## For every step: look along a ray from the mountain centre and find the outer edge of everything
## at least as tall as the step (the wall it climbs). The step sits just outside that wall,
## touching it. Upper levels are smaller, so the spiral walks inward as it climbs and ends one
## step below the peak. Where two steps end up too far apart (the wall jumped back at a ledge),
## filler steps go in between unless there is already cake to walk on.
## The spiral starts beside the waterfall and skips the fall's side until it is above the pond.
extends RefCounted

const Mountain := preload("res://choco_mountain.gd")

const RISE := 0.8  # height between steps (a hop for the character)
const WIDTH := 2.0  # step block size (x, z); the kit slab is 1 x 1 x 0.6
const RUN := 2.0  # distance along the wall between step centres
const MAX_GAP := 2.7  # step centres further apart than this get fillers
const FALL_SIDE := 0.75  # radians either side of the waterfall the low steps keep clear of

## Each step: {"top": Vector3 (centre, top height), "yaw": float, "height": float, "piece": String}
var steps: Array[Dictionary] = []


func _init(rng: RandomNumberGenerator, mountain: Mountain, pieces: Array[String]) -> void:
	var centre := mountain.centre
	var fall_angle := PI / 2  # the source block is straight in front (+Z) of the centre
	var angle := fall_angle + FALL_SIDE
	var h := RISE
	var last := Vector2.INF
	var piece := ""
	for _guard in 400:
		if h >= mountain.top() - 0.05:
			break
		var dir := Vector2(cos(angle), sin(angle))
		var edge := _wall_distance(mountain, centre, dir, h)
		var low := h < mountain.pond.y + 1.0
		if edge < 0.0 or (low and absf(angle_difference(angle, fall_angle)) < FALL_SIDE):
			angle += 0.12
			continue
		var p := centre + dir * (edge + WIDTH * 0.42)
		if last != Vector2.INF and last.distance_to(p) > MAX_GAP:
			_fill(rng, mountain, pieces, last, p, h - RISE, h)
		piece = _next_piece(rng, pieces, piece)
		_add(mountain, p, h, -angle, piece)
		last = p
		angle += RUN / maxf(edge + WIDTH * 0.42, 3.0)
		h += RISE


## How far from the centre along dir the cake is still at least h tall (-1 if it never is).
func _wall_distance(mountain: Mountain, centre: Vector2, dir: Vector2, h: float) -> float:
	var r := 18.0
	while r > 0.0:
		if mountain.height_at(centre + dir * r) >= h:
			return r
		r -= 0.2
	return -1.0


## Steps between a and b where the gap is too wide to jump, skipping spots already on cake.
func _fill(rng: RandomNumberGenerator, mountain: Mountain, pieces: Array[String], a: Vector2, b: Vector2,
		h0: float, h1: float) -> void:
	var n := ceili(a.distance_to(b) / RUN) - 1
	for i in range(1, n + 1):
		var k := float(i) / (n + 1)
		var p := a.lerp(b, k)
		var h := lerpf(h0, h1, k)
		if mountain.height_at(p) > h - 0.4:
			continue
		_add(mountain, p, h, (b - a).angle(), _next_piece(rng, pieces, ""))


## A step whose top is at h. Low steps reach down to whatever is below, so they read as little
## pillars instead of floating slabs.
func _add(mountain: Mountain, p: Vector2, h: float, yaw: float, piece: String) -> void:
	var below := mountain.height_at(p)
	var height := 0.6
	if h - below < 1.6:
		height = maxf(0.6, h - below + 0.05)
	steps.append({"top": Vector3(p.x, h, p.y), "yaw": yaw, "height": height, "piece": piece})


## A different stair piece from the last one.
func _next_piece(rng: RandomNumberGenerator, pieces: Array[String], last: String) -> String:
	var piece := last
	while piece == last:
		piece = pieces[rng.randi() % pieces.size()]
	return piece


## The kit slab is 1 x 1 x 0.6 with its base at y = 0.
static func xform(step: Dictionary) -> Transform3D:
	var size := Vector3(WIDTH, step.height / 0.6, WIDTH)
	var base: Vector3 = step.top - Vector3(0, step.height, 0)
	return Transform3D(Basis(Vector3.UP, step.yaw).scaled(size), base)
