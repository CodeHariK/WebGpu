## Picks where a foot steps: a hexagon of candidate points round the leg's (predicted) rest spot.
## Each point gets one ray straight down to find the floor, and is rejected if:
##   no floor          nothing under it (a gap, a ledge)
##   too steep         the floor there is steeper than max_slope
##   too high / low    more than max_step above or below the body's ground
##   crowded           another foot is (or is landing) within min_gap
## Of the points left, it takes:
##   moving, weighted   one of the FRONT corners (facing within ~65° of the walk direction), the
##                      straight-ahead one most likely (forward_bias vs randomness)
##   moving, random     any usable corner (to see why weighting matters: it walks drunk)
##   standing           the point nearest the rest spot (it settles instead of fidgeting)
## Ring shapes: HEX = the 6 corners; HEX_CENTRE = + the centre; TWO_RINGS = + 12 more on a second
## ring (a little hex grid: more choice round obstacles).
class_name HexPicker
extends RefCounted

enum Rings { HEX, HEX_CENTRE, TWO_RINGS }

const PROBE_UP := 1.2 ## the ray starts this far above the point …
const PROBE_DOWN := 1.6 ## … and ends this far below it
const FRONT := 0.4 ## a corner counts as "front" when its direction · walk direction ≥ this


## The candidate points round `centre` (flat), turned by `yaw` so a corner points forward.
static func points(centre: Vector3, radius: float, rings: Rings, yaw: float) -> Array[Vector3]:
	var result: Array[Vector3] = []
	for i in 6:
		result.append(centre + _flat(yaw + i * PI / 3.0) * radius)
	if rings != Rings.HEX:
		result.append(centre)
	if rings == Rings.TWO_RINGS:
		for i in 6:
			result.append(centre + _flat(yaw + i * PI / 3.0) * radius * 2.0) # outer corners
			result.append(centre + _flat(yaw + i * PI / 3.0 + PI / 6.0) * radius * sqrt(3.0)) # between them
	return result


## Fill leg.candidates and return the index of the chosen one (−1: nowhere to step).
static func pick(space: PhysicsDirectSpaceState3D, leg: HexLeg, centre: Vector3, ground_y: float, move_dir: Vector3, others: Array[Vector3], s: Dictionary, rng: RandomNumberGenerator) -> int:
	leg.centre = centre
	leg.candidates.clear()
	leg.chosen = -1
	var moving := move_dir.length() > 0.05
	var best_score := -INF
	var front_only: bool = moving and s.weighted and _any_front(centre, move_dir, s)
	for point in points(centre, s.ring_radius, s.rings, s.yaw):
		var top := point + Vector3.UP * PROBE_UP
		var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(top, point + Vector3.DOWN * PROBE_DOWN))
		var c := {"point": point, "top": top, "hit": Vector3.INF, "ok": false, "why": ""}
		c.why = _reject(hit, ground_y, others, s)
		if not hit.is_empty():
			c.hit = hit.position
		c.ok = c.why == ""
		leg.candidates.append(c)
		if not c.ok:
			continue
		if front_only and _ahead(point, centre, move_dir) < FRONT:
			continue
		var score := _score(point, centre, move_dir, moving, s, rng)
		if score > best_score:
			best_score = score
			leg.chosen = leg.candidates.size() - 1
	if leg.chosen < 0 and front_only: # every front corner was bad (a pit ahead): take any usable one
		for i in leg.candidates.size():
			if leg.candidates[i].ok:
				var score := _score(leg.candidates[i].point, centre, move_dir, moving, s, rng)
				if score > best_score:
					best_score = score
					leg.chosen = i
	return leg.chosen


static func _reject(hit: Dictionary, ground_y: float, others: Array[Vector3], s: Dictionary) -> String:
	if hit.is_empty():
		return "no floor"
	if (hit.normal as Vector3).angle_to(Vector3.UP) > deg_to_rad(s.max_slope):
		return "too steep"
	if absf(hit.position.y - ground_y) > s.max_step:
		return "too high / low"
	for other in others:
		if Vector2(other.x - hit.position.x, other.z - hit.position.z).length() < s.min_gap:
			return "crowded"
	return ""


static func _ahead(point: Vector3, centre: Vector3, move_dir: Vector3) -> float:
	var offset := Vector3(point.x - centre.x, 0.0, point.z - centre.z)
	return offset.normalized().dot(move_dir) if offset.length() > 0.01 else 0.0


# True if some corner faces forward (always, for a full hex; kept for odd ring shapes).
static func _any_front(centre: Vector3, move_dir: Vector3, s: Dictionary) -> bool:
	for point in points(centre, s.ring_radius, s.rings, s.yaw):
		if _ahead(point, centre, move_dir) >= FRONT:
			return true
	return false


static func _score(point: Vector3, centre: Vector3, move_dir: Vector3, moving: bool, s: Dictionary, rng: RandomNumberGenerator) -> float:
	var offset := Vector3(point.x - centre.x, 0.0, point.z - centre.z)
	if not moving:
		return -offset.length() + rng.randf() * 0.05
	var ahead := offset.normalized().dot(move_dir) if offset.length() > 0.01 else 0.0
	var bias: float = s.forward_bias if s.weighted else 0.0
	return ahead * bias + rng.randf() * s.randomness


static func _flat(angle: float) -> Vector3:
	return Vector3(sin(angle), 0.0, -cos(angle)) # angle 0 = −Z (forward)
