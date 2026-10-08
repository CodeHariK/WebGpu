## Finds paths on a TacticalGrid, in two passes:
##   1. A*  over cells and links, cost = each link's cost, guess = straight-line distance to the
##          goal. Gives one waypoint per cell.
##   2. string pulling  thins out the WALK waypoints a straight walk would skip anyway, so the
##          walker heads for corners, not every cell. Leaps and drops are always kept.
## A waypoint is {"position": Vector3, "cell": int, "kind": TacticalGrid.Link} — kind is how it is
## reached (walk to it, drop to it, climb or jump to it).
class_name GridPathFinder
extends RefCounted


## Waypoints from the cell nearest `from` to the one nearest `to`, start excluded. Empty if
## there's no way (or `from` and `to` are in the same cell).
static func find(grid: TacticalGrid, from: Vector3, to: Vector3) -> Array[Dictionary]:
	var start := grid.nearest_cell(from)
	var goal := grid.nearest_cell(to)
	if start < 0 or goal < 0 or start == goal:
		return []
	var came_from := _search(grid, start, goal)
	if not came_from.has(goal):
		return []
	return _string_pull(grid, from, _walk_back(grid, came_from, start, goal))


# A*: returns for every cell it reached {cell: [previous cell, link kind]}.
static func _search(grid: TacticalGrid, start: int, goal: int) -> Dictionary:
	var came_from := {}
	var cost_so_far := {start: 0.0}
	var goal_position := grid.positions[goal]
	var open := GridHeap.new()
	open.push(start, 0.0)
	while not open.is_empty():
		var current := open.pop()
		if current == goal:
			break
		for link: Array in grid.links[current]:
			var next: int = link[0]
			var cost: float = cost_so_far[current] + link[1]
			if cost < cost_so_far.get(next, INF):
				cost_so_far[next] = cost
				came_from[next] = [current, link[2]]
				open.push(next, cost + grid.positions[next].distance_to(goal_position))
	return came_from


# Follow came_from from the goal back to the start: one waypoint per cell, in walking order.
static func _walk_back(grid: TacticalGrid, came_from: Dictionary, start: int, goal: int) -> Array[Dictionary]:
	var path: Array[Dictionary] = []
	var cell := goal
	while cell != start:
		var step: Array = came_from[cell]
		path.push_front({"position": grid.positions[cell], "cell": cell, "kind": step[1]})
		cell = step[0]
	return path


# From each kept point, skip ahead to the furthest later WALK waypoint still reachable in a
# straight line; keep that one and carry on from it.
static func _string_pull(grid: TacticalGrid, from: Vector3, path: Array[Dictionary]) -> Array[Dictionary]:
	var pulled: Array[Dictionary] = []
	var anchor := from
	var i := 0
	while i < path.size():
		var furthest := i
		while furthest + 1 < path.size() and _can_skip_to(grid, anchor, path[furthest], path[furthest + 1]):
			furthest += 1
		pulled.append(path[furthest])
		anchor = path[furthest].position
		i = furthest + 1
	return pulled


static func _can_skip_to(grid: TacticalGrid, anchor: Vector3, current: Dictionary, next: Dictionary) -> bool:
	var walking: bool = current.kind == TacticalGrid.Link.WALK and next.kind == TacticalGrid.Link.WALK
	return walking and _straight_walk(grid, anchor, next.position)


# A straight walk from a to b stays on walkable ground: about the same level, clear at knee
# height, and every cell under the line — and under lines a body's width to each side — within a
# step of the last one. The centre line also mustn't run along a ledge.
static func _straight_walk(grid: TacticalGrid, a: Vector3, b: Vector3) -> bool:
	if absf(a.y - b.y) > grid.step_height * 2.0 or not grid.clear_between(a, b):
		return false
	var steps := ceili(a.distance_to(b) / (grid.cell_size * 0.5))
	var across := (b - a).cross(Vector3.UP).normalized() * grid.body_radius
	for lane: Vector3 in [Vector3.ZERO, across, -across]:
		var last_y := a.y
		for k in range(1, steps):
			var cell := grid.cell_in_column(a.lerp(b, float(k) / steps) + lane, last_y)
			if cell < 0:
				return false # off the walkable surface (beside the stairs, over a gap)
			if lane == Vector3.ZERO and grid.edge[cell] == 1 and k > 1 and k < steps - 1:
				return false # running along a ledge: keep the waypoints that steer clear of it
			last_y = grid.positions[cell].y
	return true
