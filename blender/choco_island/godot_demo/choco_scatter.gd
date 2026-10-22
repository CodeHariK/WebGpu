## Scatter: candy props and pistachio patches on the island top, frosting drips round its edge,
## floating cake platforms in the air around it (with a prop or two each), swirls on the sea.
## Keeps props off the mountain, the stairs, the stream and each other.
extends RefCounted

const Mesh_ := preload("res://choco_mesh.gd")
const Mountain := preload("res://choco_mountain.gd")
const Shape := preload("res://choco_shape.gd")

const PROP_COUNT := 42
const PLATFORM_COUNT := 5
const COLORS := ["red", "mint", "yellow", "purple"]

## {"piece": String, "xform": Transform3D}
var pieces: Array[Dictionary] = []
## Floating platforms: {"centre": Vector3 (top centre), "radius": float}
var platforms: Array[Dictionary] = []

var _taken: Array[Vector3] = []  # (x, z, radius) of things already placed


func _init(b: Mesh_.Builder, rng: RandomNumberGenerator, mountain: Mountain, shape: Shape,
		stream: Array[Vector2], stair_tops: Array[Vector2], end_angle: float) -> void:
	for p in stair_tops:
		_taken.append(Vector3(p.x, p.y, 1.2))
	_drips(rng, shape, end_angle)
	_patches(b, rng, mountain, shape, stream)
	_props(rng, mountain, shape, stream)
	_platforms(rng, shape)
	_sea_swirls(rng)


func _add(piece: String, at: Vector3, yaw: float, scale: Vector3) -> void:
	pieces.append({"piece": piece, "xform": Transform3D(Basis(Vector3.UP, yaw) * Basis.from_scale(scale), at)})


## Frosting drips every ~2 m round the edge (not where the stream pours off).
func _drips(rng: RandomNumberGenerator, shape: Shape, end_angle: float) -> void:
	var n := int(TAU * Shape.RADIUS / 2.0)
	for i in n:
		var a := TAU * i / n + rng.randf_range(-0.03, 0.03)
		if absf(angle_difference(a, end_angle)) < 0.13:
			continue
		var at := Vector2.from_angle(a) * shape.radius_at(a) * 0.975
		var s := rng.randf_range(0.7, 1.25)
		_add("Drip", Vector3(at.x, -0.5, at.y), 0.0, Vector3(s, rng.randf_range(0.45, 1.2), s))


## Flat pistachio-crumb patches (the "grass").
func _patches(b: Mesh_.Builder, rng: RandomNumberGenerator, mountain: Mountain, shape: Shape, stream: Array[Vector2]) -> void:
	for _i in 9:
		for _try in 20:
			var p := Vector2(rng.randf_range(-14, 14), rng.randf_range(-14, 14))
			var r := rng.randf_range(1.4, 3.0)
			if shape.contains(p, r + 0.5) and not mountain.near(p, r) and _stream_distance(p, stream) > r + 1.2:
				Mesh_.disc(b, "pistachio", p, r, -0.05, 0.05, Mesh_.wobbly_ring(rng, 7, 0.25))
				break


## Lollipop trees, swirl lollipops, popsicles, candy canes and gumdrops, kept apart.
func _props(rng: RandomNumberGenerator, mountain: Mountain, shape: Shape, stream: Array[Vector2]) -> void:
	var kinds := [  # weight, radius it needs, piece family
		[0.28, 1.4, "LolliTree_"], [0.1, 1.3, "Swirl_"], [0.14, 0.9, "Popsicle_"],
		[0.1, 0.8, "Cane"], [0.38, 0.6, "Gumdrop_"],
	]
	for _i in PROP_COUNT:
		var kind: Array = kinds[_weighted(rng, kinds)]
		for _try in 30:
			var p := Vector2(rng.randf_range(-15, 15), rng.randf_range(-15, 15))
			var r: float = kind[1]
			if not shape.contains(p, r) or mountain.near(p, r + 0.6) or _stream_distance(p, stream) < r + 1.4 or _blocked(p, r):
				continue
			_taken.append(Vector3(p.x, p.y, r))
			var piece: String = kind[2]
			if piece != "Cane":
				var colors: Array = COLORS + (["pink"] if piece in ["Popsicle_", "Gumdrop_"] else [])
				piece += colors[rng.randi() % colors.size()]
			var s := rng.randf_range(0.8, 1.2) if not piece.begins_with("Gumdrop") else rng.randf_range(0.6, 1.1)
			_add(piece, Vector3(p.x, 0, p.y), rng.randf_range(-0.5, 0.5), Vector3.ONE * s)
			break


## Cake slices floating round the island, each with something on top.
func _platforms(rng: RandomNumberGenerator, shape: Shape) -> void:
	var start := rng.randf_range(0, TAU)
	for i in PLATFORM_COUNT:
		var a := start + TAU * (i + rng.randf_range(-0.2, 0.2)) / PLATFORM_COUNT
		var r := rng.randf_range(2.2, 3.4)
		var d := shape.radius_at(a) + r + rng.randf_range(5.0, 9.0)
		var top := Vector3(cos(a) * d, rng.randf_range(4.0, 11.0), sin(a) * d)
		platforms.append({"centre": top, "radius": r})
		_add("Platform_a" if i % 2 == 0 else "Platform_b", top, rng.randf_range(0, TAU), Vector3.ONE * r)
		var color: String = COLORS[i % COLORS.size()]
		if i % 2 == 0:
			_add("LolliTree_" + color, top + Vector3(-0.2 * r, 0, 0), 0.0, Vector3.ONE * 0.75)
			_add("Gumdrop_" + COLORS[(i + 1) % COLORS.size()], top + Vector3(0.45 * r, 0, 0.3 * r), 0.0, Vector3.ONE * 0.8)
		else:
			_add("Swirl_" + color, top + Vector3(0, 0, -0.2 * r), rng.randf_range(-0.4, 0.4), Vector3.ONE * 0.85)
			_add("Cane", top + Vector3(0.5 * r, 0, 0.4 * r), rng.randf_range(0, TAU), Vector3.ONE * 0.7)


## Cream and light syrup arcs drifting on the sea, clear of the platforms.
func _sea_swirls(rng: RandomNumberGenerator) -> void:
	for i in 24:
		for _try in 30:
			var a := rng.randf_range(0, TAU)
			var p := Vector2.from_angle(a) * rng.randf_range(20.0, 46.0)
			if _near_platform(p):
				continue
			var s := rng.randf_range(1.5, 4.5)
			_add("SeaSwirl_cream" if i % 3 == 0 else "SeaSwirl_syrup", Vector3(p.x, -2.88, p.y), rng.randf_range(0, TAU), Vector3(s, 1, s))
			break


func _near_platform(p: Vector2) -> bool:
	for pl in platforms:
		if p.distance_to(Vector2(pl.centre.x, pl.centre.z)) < pl.radius + 3.0:
			return true
	return false


func _blocked(p: Vector2, r: float) -> bool:
	for t in _taken:
		if p.distance_to(Vector2(t.x, t.y)) < r + t.z:
			return true
	return false


static func _stream_distance(p: Vector2, stream: Array[Vector2]) -> float:
	var best := INF
	for i in stream.size() - 1:
		var q := Geometry2D.get_closest_point_to_segment(p, stream[i], stream[i + 1])
		best = minf(best, p.distance_to(q))
	return best


static func _weighted(rng: RandomNumberGenerator, kinds: Array) -> int:
	var x := rng.randf()
	for i in kinds.size():
		x -= kinds[i][0]
		if x <= 0.0:
			return i
	return kinds.size() - 1
