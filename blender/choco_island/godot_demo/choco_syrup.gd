## The syrup: the pond on the source block, the fall down its face, the pool at the bottom, a stream
## that wanders to the island edge, and a second fall off the edge into the sea.
## Builds the meshes into a Builder and lists the kit pieces (foam, bubbles, bridge, rocks) it wants.
## Surfaces: "river" (stream), "fall" (both falls), "pool" (pond and pool) — each gets the
## chocolate river shader with its own speed (choco_palette.gd).
extends RefCounted

const Mesh_ := preload("res://choco_mesh.gd")
const Mountain := preload("res://choco_mountain.gd")
const Shape := preload("res://choco_shape.gd")

const STREAM_STEP := 3.0
const SEA_Y := -3.0

var stream: Array[Vector2] = []  # centre line on the island top
var pieces: Array[Dictionary] = []  # {"piece": String, "xform": Transform3D}
var end_angle := 0.0  # where the stream leaves the island (no frosting drips there)


func _init(b: Mesh_.Builder, rng: RandomNumberGenerator, mountain: Mountain, shape: Shape) -> void:
	var pond := mountain.pond
	Mesh_.disc(b, "pool", Vector2(pond.x, pond.z), Mountain.POND_RADIUS, pond.y - 0.1, pond.y + 0.08,
		Mesh_.wobbly_ring(rng, 9, 0.1))
	var pool := Vector2(pond.x, mountain.lip_z() + 2.4)
	_fall(b, mountain, pond)
	Mesh_.disc(b, "pool", pool, 2.0, -0.05, 0.1, Mesh_.wobbly_ring(rng, 11, 0.12))
	_splash(rng, Vector3(pond.x, 0.12, mountain.lip_z() + 1.1), 1.3)
	_stream(b, rng, mountain, shape, pool)
	_edge_fall(b, rng, shape)


## Over the lip of the source block and straight down its face into the pool.
func _fall(b: Mesh_.Builder, mountain: Mountain, pond: Vector3) -> void:
	var lip := mountain.lip_z()
	var top := mountain.source.top()
	var profile := [  # (z, y)
		Vector2(pond.z, top + 0.05), Vector2(lip - 0.2, top + 0.08), Vector2(lip + 0.25, top - 0.15),
		Vector2(lip + 0.45, top - 0.8), Vector2(lip + 0.55, top * 0.65), Vector2(lip + 0.6, top * 0.4),
		Vector2(lip + 0.65, top * 0.18), Vector2(lip + 0.85, 0.35), Vector2(lip + 1.2, 0.08),
	]
	var points: Array[Vector3] = []
	var widths: Array[float] = []
	for i in profile.size():
		points.append(Vector3(pond.x, profile[i].y, profile[i].x))
		widths.append(2.1 + 0.9 * i / (profile.size() - 1))
	var side := Vector3.BACK.cross(Vector3.UP)
	Mesh_.ribbon(b, "fall", points, widths, 0.26, side, 0.12)


## From the pool forward, wandering a little, until it reaches the edge.
func _stream(b: Mesh_.Builder, rng: RandomNumberGenerator, mountain: Mountain, shape: Shape, pool: Vector2) -> void:
	var p := pool
	var heading := PI / 2 + rng.randf_range(-0.4, 0.4)
	stream.append(p)
	for _i in 12:
		heading = clampf(heading + rng.randf_range(-0.45, 0.45), PI / 2 - 0.9, PI / 2 + 0.9)
		var next := p + Vector2(cos(heading), sin(heading)) * STREAM_STEP
		if not shape.contains(next, 0.5):
			break
		p = next
		stream.append(p)
	var dir := (p - stream[maxi(0, stream.size() - 2)]).normalized()
	end_angle = (p + dir * 2.0).angle()
	stream.append(Vector2.from_angle(end_angle) * shape.radius_at(end_angle) * 0.985)
	var points: Array[Vector3] = []
	var widths: Array[float] = []
	for i in stream.size():
		points.append(Vector3(stream[i].x, 0.04, stream[i].y))
		widths.append(2.4 if i == 0 else rng.randf_range(1.6, 2.0))
	Mesh_.ribbon(b, "river", points, widths, 0.18, Vector3.ZERO, 0.04)
	_bridge_and_rocks(rng)


## A wafer bridge across the middle of the stream, chocolate rocks along its banks.
func _bridge_and_rocks(rng: RandomNumberGenerator) -> void:
	var mid := stream.size() / 2
	var a := stream[mid - 1]
	var c := stream[mid]
	var at := (a + c) * 0.5
	var across := (c - a).angle() + PI / 2
	pieces.append({"piece": "Bridge", "xform": Transform3D(Basis(Vector3.UP, -across), Vector3(at.x, 0, at.y))})
	for i in range(1, stream.size() - 1):
		var t := (stream[i + 1] - stream[i - 1]).normalized()
		var n := Vector2(-t.y, t.x)
		for s in [-1.0, 1.0]:
			if rng.randf() < 0.3 or stream[i].distance_to(at) < 2.5:
				continue
			var q: Vector2 = stream[i] + n * s * rng.randf_range(1.2, 1.6) + t * rng.randf_range(-0.8, 0.8)
			var size := rng.randf_range(0.7, 1.2)
			var piece := "Rock_a" if rng.randf() < 0.5 else "Rock_b"
			pieces.append({"piece": piece, "xform": Transform3D(
				Basis(Vector3.UP, rng.randf_range(0, TAU)).scaled(Vector3.ONE * size), Vector3(q.x, 0, q.y))})


## Off the edge: out a little, then down into the sea, with foam where it lands.
func _edge_fall(b: Mesh_.Builder, rng: RandomNumberGenerator, shape: Shape) -> void:
	var d := Vector3(cos(end_angle), 0, sin(end_angle))
	var r := shape.radius_at(end_angle)
	var profile := [Vector2(-0.6, 0.04), Vector2(0.15, 0.0), Vector2(0.45, -0.4), Vector2(0.6, -1.2),
		Vector2(0.66, -2.2), Vector2(0.7, SEA_Y - 0.1)]
	var points: Array[Vector3] = []
	var widths: Array[float] = []
	for i in profile.size():
		points.append(d * (r + profile[i].x) + Vector3(0, profile[i].y, 0))
		widths.append(1.8 + 0.1 * i)
	Mesh_.ribbon(b, "fall", points, widths, 0.24, d.cross(Vector3.UP), 0.1)
	_splash(rng, d * (r + 0.95) + Vector3(0, SEA_Y + 0.12, 0), 1.1)


## A foam ring and a few bubbles where syrup lands.
func _splash(rng: RandomNumberGenerator, at: Vector3, radius: float) -> void:
	pieces.append({"piece": "FoamRing", "xform": Transform3D(Basis.from_scale(Vector3(radius, 1, radius)), at)})
	for _i in 9:
		var a := rng.randf_range(0, TAU)
		var off := Vector3(cos(a), 0, sin(a)) * rng.randf_range(0.4, radius * 1.3)
		pieces.append({"piece": "Bubble", "xform": Transform3D(Basis.from_scale(Vector3.ONE * rng.randf_range(0.6, 1.4)), at + off)})
