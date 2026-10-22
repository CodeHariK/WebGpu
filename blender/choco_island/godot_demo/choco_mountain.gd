## The mountain: cake cubes of different sizes stacked level by level, each block resting on a
## bigger one of the level below, narrowing up to a peak block that gets a cherry.
##
## The first block is the SOURCE: the biggest, square to the front, with a syrup pond on its top
## front corner. Nothing is stacked over the pond and nothing is built in front of its face, so
## the waterfall has a clear drop.
extends RefCounted

const GROUND_BLOCKS := 7
const LEVEL_COUNTS := [GROUND_BLOCKS, 5, 3, 1]  # blocks per level, ground first
const CAP := 1.03  # a block's frosting top is at base + size * CAP
const POND_RADIUS := 1.2


## One cake cube: centre (x, z) with its base height in pos.y, edge length, turn, kit piece.
class Block:
	var pos: Vector3
	var size: float
	var yaw: float
	var piece: String
	var level: int

	func top() -> float:
		return pos.y + size * 1.03

	func centre() -> Vector2:
		return Vector2(pos.x, pos.z)

	## Is (x, z) over this block's (slightly shrunk) square footprint?
	func covers(p: Vector2, shrink := 0.95) -> bool:
		var local := (p - centre()).rotated(yaw)
		var h := size * 0.5 * shrink
		return absf(local.x) <= h and absf(local.y) <= h

	## A transform for the kit cube (unit size, base at y = 0).
	func xform() -> Transform3D:
		return Transform3D(Basis(Vector3.UP, yaw).scaled(Vector3.ONE * size), pos)


var blocks: Array[Block] = []
var source: Block
var pond := Vector3.ZERO  # pond centre (x, top y, z)
var centre := Vector2.ZERO
var peak: Block


func _init(rng: RandomNumberGenerator, kit_cubes: Array[String], at: Vector2) -> void:
	centre = at
	_place_source(rng, kit_cubes)
	_place_ground(rng, kit_cubes)
	for level in range(1, LEVEL_COUNTS.size()):
		_place_level(rng, kit_cubes, level)
	_find_peak()


## Height of the top surface at (x, z): the highest block over it, or 0 (the island top).
func height_at(p: Vector2) -> float:
	var h := 0.0
	for b in blocks:
		if b.covers(p):
			h = maxf(h, b.top())
	return h


func top() -> float:
	return peak.top()


## True when (x, z) is under or right next to the mountain.
func near(p: Vector2, margin: float) -> bool:
	for b in blocks:
		if b.covers(p, 1.0 + margin * 2.0 / b.size):
			return true
	return false


## The z of the source block's front face (where the syrup goes over).
func lip_z() -> float:
	return source.pos.z + source.size * 0.525


func _place_source(rng: RandomNumberGenerator, kit_cubes: Array[String]) -> void:
	source = _add(Vector3(centre.x, 0, centre.y + 2.5), rng.randf_range(5.6, 6.6), 0.0, _pick(rng, kit_cubes, "Cube_sponge_pink"))
	pond = Vector3(source.pos.x + rng.randf_range(-1.0, 1.0), source.top(), lip_z() - POND_RADIUS - 0.45)


func _place_ground(rng: RandomNumberGenerator, kit_cubes: Array[String]) -> void:
	var placed := 0
	for _try in 200:
		if placed == GROUND_BLOCKS:
			break
		var s := rng.randf_range(3.6, 6.4)
		var p := centre + Vector2(rng.randf_range(-9.0, 9.0), rng.randf_range(-6.0, 2.0))
		if _crowded(p, s, 0.55, 0.0) or _blocks_fall(p, s):
			continue
		_add(Vector3(p.x, 0, p.y), s, rng.randf_range(-0.5, 0.5), _pick(rng, kit_cubes, "Cube_"))
		placed += 1


## One level up: each new block sits on a block of the level below (central ones more often), a
## bit smaller than it and nudged toward the middle, so the pile narrows into a mountain.
func _place_level(rng: RandomNumberGenerator, kit_cubes: Array[String], level: int) -> void:
	var below: Array[Block] = []
	for b in blocks:
		if b.level == level - 1:
			below.append(b)
	if below.is_empty():
		return
	below.sort_custom(func(x: Block, y: Block) -> bool: return x.centre().distance_to(centre) < y.centre().distance_to(centre))
	var count: int = LEVEL_COUNTS[level]
	var placed := 0
	for _try in 200:
		if placed == count:
			break
		var under := below[int(pow(rng.randf(), 2.0) * below.size())]
		var s := under.size * rng.randf_range(0.55, 0.8)
		var inward := (centre - under.centre()).normalized() * under.size * 0.15
		var p := under.centre() + inward + Vector2(rng.randf_range(-1, 1), rng.randf_range(-1, 1)) * under.size * 0.28
		var base := _support(p, s, level)
		if base <= 0.5 or _crowded(p, s, 0.5, base) or _over_pond(p, s):
			continue
		var cap := "white" if base > 9.0 else "pink"
		var block := _add(Vector3(p.x, base - 0.04 * s, p.y), s, rng.randf_range(-0.6, 0.6), _pick(rng, kit_cubes, "Cube_", cap))
		block.level = level
		placed += 1


## The highest block is the peak (the island puts the cherry on it).
func _find_peak() -> void:
	peak = blocks[0]
	for b in blocks:
		if b.top() > peak.top():
			peak = b


## Where a block of size s at p would rest: on the highest block under its middle, which must
## belong to the level below (so levels form terraces instead of piling into a spire), and with
## most of the new block supported (none hanging more than ~half off an edge). 0 = can't go here.
func _support(p: Vector2, s: float, level: int) -> float:
	var hi := 0.0
	var lo := INF
	for o in [Vector2.ZERO, Vector2(1, 1), Vector2(-1, 1), Vector2(1, -1), Vector2(-1, -1)]:
		var h := height_at(p + o * s * 0.3)
		hi = maxf(hi, h)
		lo = minf(lo, h)
	var under := _highest_at(p)
	if under == null or under.level != level - 1 or lo < hi - 0.55 * s:
		return 0.0
	return hi


func _highest_at(p: Vector2) -> Block:
	var best: Block = null
	for b in blocks:
		if b.covers(p) and (best == null or b.top() > best.top()):
			best = b
	return best


## Too close to a block standing at about the same height (blocks on one level shouldn't merge).
func _crowded(p: Vector2, s: float, k: float, base: float) -> bool:
	for b in blocks:
		if absf(b.pos.y - base) < 0.3 * s and p.distance_to(b.centre()) < k * (s + b.size):
			return true
	return false


## Would a ground block at p stand in front of the source face (in the waterfall's way)?
func _blocks_fall(p: Vector2, s: float) -> bool:
	var beside := absf(p.x - source.pos.x) < (source.size + s) * 0.5 + 2.5
	return beside and p.y + s * 0.5 > source.pos.z + source.size * 0.5 - 1.2


func _over_pond(p: Vector2, s: float) -> bool:
	return p.distance_to(Vector2(pond.x, pond.z)) < s * 0.72 + POND_RADIUS + 0.5


func _add(pos: Vector3, s: float, yaw: float, piece: String) -> Block:
	var b := Block.new()
	b.pos = pos
	b.size = s
	b.yaw = yaw
	b.piece = piece
	blocks.append(b)
	return b


## A random kit cube whose name starts with `prefix` (and contains `cap` if given).
static func _pick(rng: RandomNumberGenerator, kit_cubes: Array[String], prefix: String, cap := "") -> String:
	var options: Array[String] = []
	for piece in kit_cubes:
		if piece.begins_with(prefix) and (cap == "" or piece.contains("_" + cap + "_")):
			options.append(piece)
	return options[rng.randi() % options.size()]
