## The AI's map of a fight area: a layered (2.5D) grid scanned from the physics world — no navmesh.
##
## The area is split into columns `cell_size` apart. A column holds one cell per walkable surface
## in it, so floors above each other (ground, balcony, roof) each get their own cell. Cells are
## joined by links that say how a creature gets from one to the other:
##   WALK    a neighbour within a step up or down (stairs and ramps join floors this way)
##   DROP    off a ledge, down only
##   LADDER  / JUMP  hand-placed with GridLink marker pairs
##
## This file holds the data and the small queries. The work is done elsewhere:
##   GridScanner     scan()      builds the cells and links
##   GridPathFinder  find_path() A* plus string pulling, returns waypoints
## Everything the AI learns later (what the player can see, cover, squad claims) goes per cell.
class_name TacticalGrid
extends RefCounted

enum Link { WALK, DROP, LADDER, JUMP }

# The area (set before scan()).
var origin := Vector3.ZERO ## minimum corner (x, z); y = the lowest point scanned
var size := Vector2i(32, 32) ## columns along x and z
var cell_size := 0.75
var height := 8.0 ## scan from origin.y + height down to origin.y
var collision_mask := 1

# The creature (set before scan()).
var step_height := 0.55 ## the most a WALK link climbs or descends
var max_drop := 3.2 ## the furthest a DROP link falls
var headroom := 0.9 ## free space a cell needs above it
var body_radius := 0.3 ## free space a cell needs around it, at body height
var body_height := 0.65 ## where that body check sits above the cell
var knee_height := 0.4 ## height of the ray that checks the way between two cells is clear
var edge_probe := 0.45 ## no ground this far out on some side = an edge cell
var edge_cost := 2.0 ## extra cost to walk into an edge cell: paths keep off ledges and stair sides

# The cells (a cell id indexes all of these).
var positions := PackedVector3Array() ## where each cell's surface is
var links: Array = [] ## per cell: Array of [to: int, cost: float, kind: Link]
var edge := PackedByteArray() ## per cell: 1 = next to a drop
var columns: Array[PackedInt32Array] = [] ## per column (column_index()): its cell ids, bottom to top

# Last scan, for the HUD.
var scan_msec := 0.0
var ray_count := 0

var space: PhysicsDirectSpaceState3D ## the physics world scanned (and used for path checks)


## Build every cell and link from the physics world. `markers` = the GridLink nodes to use.
func scan(world: PhysicsDirectSpaceState3D, markers: Array[Node] = []) -> void:
	var start := Time.get_ticks_usec()
	space = world
	ray_count = 0
	GridScanner.new(self).scan(markers)
	scan_msec = (Time.get_ticks_usec() - start) / 1000.0


## Waypoints from near `from` to near `to` (see GridPathFinder). Empty if there's no way.
func find_path(from: Vector3, to: Vector3) -> Array[Dictionary]:
	return GridPathFinder.find(self, from, to)


func cell_count() -> int:
	return positions.size()


func link_count() -> int:
	var total := 0
	for cell_links: Array in links:
		total += cell_links.size()
	return total


## The cell a creature at `point` is on: in its column or a neighbouring one, the surface closest
## to it, preferring one at or below it. −1 if none is near.
func nearest_cell(point: Vector3) -> int:
	var best := -1
	var best_score := INF
	var centre := column_of(point)
	for dz in range(-1, 2):
		for dx in range(-1, 2):
			var column := centre + Vector2i(dx, dz)
			if not in_area(column):
				continue
			for cell in columns[column_index(column)]:
				var offset := positions[cell] - point
				var above := maxf(offset.y - 0.3, 0.0) * 4.0 # a surface over your head is a poor match
				var score := Vector2(offset.x, offset.z).length() + absf(offset.y) + above
				if score < best_score:
					best_score = score
					best = cell
	return best


## The cell in `point`'s own column within a step of height `near_y`; −1 if none.
func cell_in_column(point: Vector3, near_y: float) -> int:
	var column := column_of(point)
	if not in_area(column):
		return -1
	for cell in columns[column_index(column)]:
		if absf(positions[cell].y - near_y) <= step_height:
			return cell
	return -1


## True if nothing blocks the way from a to b at knee height (a wall, a crate's side).
func clear_between(a: Vector3, b: Vector3) -> bool:
	return ray(a + Vector3.UP * knee_height, b + Vector3.UP * knee_height).is_empty()


## A physics ray against the grid's collision mask (counted in ray_count).
func ray(from: Vector3, to: Vector3) -> Dictionary:
	ray_count += 1
	return space.intersect_ray(PhysicsRayQueryParameters3D.create(from, to, collision_mask))


# --- columns ------------------------------------------------------------------------------------

func column_of(point: Vector3) -> Vector2i:
	return Vector2i(floori((point.x - origin.x) / cell_size), floori((point.z - origin.z) / cell_size))


## (x, z) of a column's centre.
func column_centre(column: Vector2i) -> Vector2:
	return Vector2(origin.x + (column.x + 0.5) * cell_size, origin.z + (column.y + 0.5) * cell_size)


func column_index(column: Vector2i) -> int:
	return column.x + column.y * size.x


func in_area(column: Vector2i) -> bool:
	return column.x >= 0 and column.y >= 0 and column.x < size.x and column.y < size.y
