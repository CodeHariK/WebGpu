## Picks where a hand or foot actually lands, out of several candidate spots (the hex spider's
## idea, extended to walls, edges and ceilings). Run once per step, not every frame.
##
## Candidates, all round the limb's home on the surface frame's plane:
##   ring     the home, 6 points round it at `radius`, and 2 points pulled back toward the body
##            (so near an edge there is always something on this side of it)
## Rays per candidate (up to three, stopping at the first hit):
##   down     along -up from just above the point: the surface it stands on
##   hook     from just under the point back toward the body: past an outside edge this hits the
##            face below it, so the hand hooks over the edge instead of grabbing air
##   reach    from the shoulder/hip straight at the point: a wall or step in the way (inside
##            corners), or the surface round a corner the other rays can't see
## A hit is rejected if:
##   too far   the shoulder/hip can't reach it (> reach × max_reach)
##   wrong way its surface faces away from the body (the underside of what it stands on)
##   crowded   another limb is planted or landing within `min_gap`
## Of the rest it takes the cheapest: distance from home, plus a cost for a surface turned away
## from the body's up (a step onto a wall is fine, but a flat spot wins), plus a little for a
## hook or reach hit (a fallback, not the first choice).
##
## Its rays go to the shared CrawlerRays log; the candidates and verdicts are kept here, for
## crawler_debug.gd.
class_name CrawlerFoothold
extends RefCounted

enum Verdict { CHOSEN, OK, MISS, TOO_FAR, WRONG_WAY, CROWDED }

var enabled := true ## false: one ray straight down at the home, nothing else (the old way)
var radius := 0.2 ## ring radius (m)
var max_reach := 0.95 ## share of the limb's full reach a spot may be at
var min_gap := 0.15 ## m between two hands/feet
var tilt_cost := 0.35 ## cost of a surface fully turned away from up (× (1 − dot))
var fallback_cost := 0.15 ## extra cost of a hook / reach hit
var world_mask := 1

var rays: CrawlerRays ## every ray goes through this log
## Debug record per limb: candidates [[point, Verdict], ...] of its last search
var candidates: Array = [[], [], [], []]


class Pick:
	var position: Vector3
	var normal: Vector3
	var hit: bool


## The best spot for `limb` round `home` (a point on the surface frame's plane). `body` is the
## posed body, `others` where the other limbs are planted or landing. Never null: with nothing
## usable it returns the home itself as a miss (the limb reaches into the air).
func pick(space: PhysicsDirectSpaceState3D, limb: int, home: Vector3, frame: Transform3D, body: Transform3D, others: PackedVector3Array) -> Pick:
	candidates[limb] = []
	var up := frame.basis.y
	var root: Vector3 = body * CrawlerLayout.ROOTS[limb]
	var best := Pick.new()
	best.position = home
	best.normal = up
	best.hit = false
	var best_cost := INF
	var best_index := -1
	var points: Array[Vector3] = [home]
	if enabled:
		points = _ring(home, frame, root)
	for point in points:
		var found := _probe(space, limb, point, up, root, frame.origin)
		var verdict := Verdict.MISS
		if not found.is_empty():
			verdict = _judge(limb, found, root, up, others)
		candidates[limb].append([found.get("position", point), verdict])
		if verdict != Verdict.OK:
			continue
		var cost: float = (found.position as Vector3).distance_to(home) + (1.0 - (found.normal as Vector3).dot(up)) * tilt_cost
		cost += fallback_cost if found.fallback else 0.0
		if cost < best_cost:
			best_cost = cost
			best_index = candidates[limb].size() - 1
			best.position = found.position
			best.normal = found.normal
			best.hit = true
	if best_index >= 0:
		candidates[limb][best_index][1] = Verdict.CHOSEN
	return best


## The points to try: home, a hex round it, two pulled back toward the body.
func _ring(home: Vector3, frame: Transform3D, root: Vector3) -> Array[Vector3]:
	var up := frame.basis.y
	var points: Array[Vector3] = [home]
	for i in 6:
		var angle := i * PI / 3.0
		points.append(home + (frame.basis.x * cos(angle) + frame.basis.z * sin(angle)) * radius)
	var inward := root - home
	inward -= up * inward.dot(up)
	for share in [0.3, 0.55]:
		points.append(home + inward * share)
	return points


## Down, then hook, then reach. {position, normal, fallback} or {} when all three miss.
func _probe(space: PhysicsDirectSpaceState3D, limb: int, point: Vector3, up: Vector3, root: Vector3, centre: Vector3) -> Dictionary:
	var hit := _ray(space, point + up * 0.45, point - up * 0.6)
	if not hit.is_empty():
		return {"position": hit.position, "normal": hit.normal, "fallback": false}
	var back := centre - point
	back -= up * back.dot(up)
	if back.length() > 0.01:
		var under := point - up * 0.12
		hit = _ray(space, under, under + back.normalized() * 0.7)
		if not hit.is_empty():
			return {"position": hit.position, "normal": hit.normal, "fallback": true}
	hit = _ray(space, root, root + (point - root).normalized() * CrawlerLayout.reach(limb))
	if not hit.is_empty():
		return {"position": hit.position, "normal": hit.normal, "fallback": true}
	return {}


func _judge(limb: int, found: Dictionary, root: Vector3, up: Vector3, others: PackedVector3Array) -> Verdict:
	var at: Vector3 = found.position
	if root.distance_to(at) > CrawlerLayout.reach(limb) * max_reach:
		return Verdict.TOO_FAR
	if (found.normal as Vector3).dot(up) < -0.3:
		return Verdict.WRONG_WAY
	for other in others:
		if other.distance_to(at) < min_gap:
			return Verdict.CROWDED
	return Verdict.OK


func _ray(space: PhysicsDirectSpaceState3D, from: Vector3, to: Vector3) -> Dictionary:
	return rays.cast(space, from, to, world_mask, CrawlerRays.Kind.FOOTHOLD)
