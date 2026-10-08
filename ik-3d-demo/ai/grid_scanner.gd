## Builds a TacticalGrid's cells and links from the physics world, in four passes:
##   1. columns   cast down each column; every flat surface with headroom and body room becomes a
##                cell (several per column when there are floors above each other)
##   2. edges     mark cells with no ground just beside them (ledges, the sides of stairs)
##   3. links     join each cell to its 8 neighbours: WALK when within a step (stairs link floors
##                this way), else a one-way DROP to the highest surface below a ledge
##   4. markers   LADDER / JUMP links from GridLink marker pairs
class_name GridScanner
extends RefCounted

const DIRECTIONS: Array[Vector2i] = [
	Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1),
	Vector2i(1, 1), Vector2i(1, -1), Vector2i(-1, 1), Vector2i(-1, -1),
]
const MAX_STACK := 8 ## at most this many surfaces stacked in one column
const FLAT := 0.7 ## a surface is walkable if its normal's y is above this

var grid: TacticalGrid


func _init(target: TacticalGrid) -> void:
	grid = target


func scan(markers: Array[Node]) -> void:
	grid.positions.clear()
	grid.columns.clear()
	grid.links.clear()
	for z in grid.size.y:
		for x in grid.size.x:
			grid.columns.append(_scan_column(Vector2i(x, z)))
	grid.edge.resize(grid.cell_count())
	for cell in grid.cell_count():
		grid.links.append([])
		grid.edge[cell] = 1 if _is_edge(grid.positions[cell]) else 0
	for cell in grid.cell_count():
		_link_neighbours(cell)
	for marker in markers:
		_add_marker_link(marker as GridLink)


# --- 1. columns ---------------------------------------------------------------------------------

# Cast down, keep a hit if it's walkable, then cast again from just under it (a ray that starts
# inside a solid passes out of it) until nothing is left below. Returns the new cell ids, bottom
# to top.
func _scan_column(column: Vector2i) -> PackedInt32Array:
	var centre := grid.column_centre(column)
	var top := grid.origin.y + grid.height
	var bottom := grid.origin.y - 0.1
	var surfaces: Array[Vector3] = []
	for attempt in MAX_STACK:
		var hit := grid.ray(Vector3(centre.x, top, centre.y), Vector3(centre.x, bottom, centre.y))
		if hit.is_empty():
			break
		var point: Vector3 = hit.position
		if hit.normal.y > FLAT and _has_room(point):
			surfaces.push_front(point)
		top = point.y - 0.05
	var ids := PackedInt32Array()
	for point in surfaces:
		ids.append(grid.cell_count())
		grid.positions.append(point)
	return ids


# Headroom straight up, and nothing within body_radius at body height.
func _has_room(point: Vector3) -> bool:
	if not grid.ray(point + Vector3.UP * 0.05, point + Vector3.UP * grid.headroom).is_empty():
		return false
	var sphere := SphereShape3D.new()
	sphere.radius = grid.body_radius
	var query := PhysicsShapeQueryParameters3D.new()
	query.shape = sphere
	query.transform = Transform3D(Basis.IDENTITY, point + Vector3.UP * grid.body_height)
	query.collision_mask = grid.collision_mask
	grid.ray_count += 1
	return grid.space.intersect_shape(query, 1).is_empty()


# --- 2. edges -----------------------------------------------------------------------------------

# Ground missing (more than a step down) edge_probe out on any of four sides.
func _is_edge(point: Vector3) -> bool:
	for side: Vector3 in [Vector3.RIGHT, Vector3.LEFT, Vector3.FORWARD, Vector3.BACK]:
		var probe := point + side * grid.edge_probe
		if grid.ray(probe + Vector3.UP * grid.step_height, probe + Vector3.DOWN * grid.step_height).is_empty():
			return true
	return false


# --- 3. links -----------------------------------------------------------------------------------

func _link_neighbours(cell: int) -> void:
	var here := grid.positions[cell]
	var column := grid.column_of(here)
	for direction in DIRECTIONS:
		var next_column := column + direction
		if not grid.in_area(next_column):
			continue
		var walk := _walkable_neighbour(here, next_column)
		if walk >= 0:
			var climb := absf(grid.positions[walk].y - here.y)
			var cost := here.distance_to(grid.positions[walk]) + climb + grid.edge[walk] * grid.edge_cost
			grid.links[cell].append([walk, cost, TacticalGrid.Link.WALK])
			continue
		var drop := _drop_below(here, next_column)
		if drop >= 0:
			grid.links[cell].append([drop, here.distance_to(grid.positions[drop]) + 1.0, TacticalGrid.Link.DROP])


# The cell in `column` within a step of `here` with nothing in the way at knee height; −1 if none.
func _walkable_neighbour(here: Vector3, column: Vector2i) -> int:
	var found := -1
	for other in grid.columns[grid.column_index(column)]:
		var point := grid.positions[other]
		if absf(point.y - here.y) <= grid.step_height and grid.clear_between(here, point):
			found = other
	return found


# The highest cell in `column` more than a step but at most max_drop below `here`, reachable by
# stepping off the ledge and falling; −1 if none.
func _drop_below(here: Vector3, column: Vector2i) -> int:
	var found := -1
	for other in grid.columns[grid.column_index(column)]:
		var fall := here.y - grid.positions[other].y
		if fall > grid.step_height and fall <= grid.max_drop and (found < 0 or grid.positions[other].y > grid.positions[found].y):
			found = other
	if found >= 0 and not _clear_ledge(here, grid.positions[found]):
		return -1
	return found


# Clear at the upper level out over the lower cell, then a clear fall down to it.
func _clear_ledge(here: Vector3, below: Vector3) -> bool:
	var over := Vector3(below.x, here.y, below.z)
	if not grid.clear_between(here, over):
		return false
	return grid.ray(over + Vector3.UP * grid.knee_height, below + Vector3.UP * 0.1).is_empty()


# --- 4. markers ---------------------------------------------------------------------------------

func _add_marker_link(marker: GridLink) -> void:
	if marker == null or marker.other == null:
		return
	var a := grid.nearest_cell(marker.global_position)
	var b := grid.nearest_cell(marker.other.global_position)
	if a < 0 or b < 0 or a == b:
		return
	var kind := TacticalGrid.Link.LADDER if marker.kind == GridLink.Kind.LADDER else TacticalGrid.Link.JUMP
	var cost := grid.positions[a].distance_to(grid.positions[b]) + marker.cost
	grid.links[a].append([b, cost, kind])
	if marker.two_way:
		grid.links[b].append([a, cost, kind])
