## Where to wait for a car: a cell beside the road ahead of it, hidden, that a spider can reach
## before the car gets there.
## For a lead time T: the road point the car reaches in T seconds is the target. Candidates are
## cells NEAR..FAR metres from it, at least OFF_ROAD from any part of the road (beside it, not on
## it), in the spider's region, away from squadmates' spots. Each is scored:
##   hidden from the car now, and from where the car will be 1.5 s before the spot (on its approach)
##   cover right next to it (a wall / rock between it and the road)
##   close to the target road point (a short leap out)
##   reachable in time: straight-line distance / spider speed must leave SPARE seconds
## If nothing works for T, later lead times are tried (further down the road).
class_name AmbushPlanner
extends RefCounted

const NEAR := 2.5 ## metres from the road point …
const FAR := 6.5 ## … to here
const OFF_ROAD := 2.6 ## metres from the road's centre line (half the road + a bit)
const SPARE := 0.8 ## seconds to spare when getting there
const MAX_RAYS := 60
const SPACING := 4.0 ## metres from squadmates' spots
const EYE := 0.6 ## the spider's eye above the cell


## {cell, offset (the road offset it ambushes)}; empty when there's nowhere.
static func find(grid: TacticalGrid, car: TestCar, lead: float, spider: Vector3, spider_speed: float, claimed: Array[Vector3]) -> Dictionary:
	for extra: float in [0.0, 2.0, 4.0, 6.0, 8.0]:
		var found := _find_at(grid, car, lead + extra, spider, spider_speed, claimed)
		if not found.is_empty():
			return found
	return {}


static func _find_at(grid: TacticalGrid, car: TestCar, lead: float, spider: Vector3, spider_speed: float, claimed: Array[Vector3]) -> Dictionary:
	var target := car.predict(lead)
	var target_offset := car.offset_of(target)
	var approach := car.predict(maxf(lead - 1.5, 0.0)) + Vector3.UP * 0.4
	var now := car.eye()
	var home := grid.nearest_cell(spider)
	var candidates: Array = []
	for cell in grid.cell_count():
		var flat := grid.positions[cell] - target
		flat.y = 0.0
		var distance := flat.length()
		if distance < NEAR or distance > FAR or (home >= 0 and grid.region[cell] != grid.region[home]):
			continue
		var travel := grid.positions[cell].distance_to(spider) / maxf(spider_speed, 0.1)
		if travel + SPARE > lead:
			continue
		var on_road := car.road.get_closest_point(grid.positions[cell]) - grid.positions[cell]
		if Vector2(on_road.x, on_road.z).length() < OFF_ROAD:
			continue
		if claimed.any(func(other: Vector3) -> bool: return other.distance_to(grid.positions[cell]) < SPACING):
			continue
		candidates.append([distance, cell])
	candidates.sort_custom(func(a: Array, b: Array) -> bool: return a[0] < b[0])
	var best := -1
	var best_score := -INF
	var rays := 0
	for candidate: Array in candidates:
		if rays >= MAX_RAYS:
			break
		var cell: int = candidate[1]
		var at := grid.positions[cell] + Vector3.UP * EYE
		var hidden_now := not grid.ray(at, now).is_empty()
		var hit := grid.ray(at, approach)
		rays += 2
		if not hidden_now and hit.is_empty():
			continue # in plain sight the whole way in: no ambush
		var score: float = (3.0 if hidden_now else 0.0) - candidate[0] * 0.4
		if not hit.is_empty():
			score += 3.0
			if at.distance_to(hit.position) < 1.5:
				score += 1.0 # cover right beside it
		if score > best_score:
			best_score = score
			best = cell
	if best < 0:
		return {}
	return {"cell": best, "offset": target_offset}
