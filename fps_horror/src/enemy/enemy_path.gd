## The enemy's route: a list of waypoints from the navmesh, some of them lifted onto walls or the
## ceiling. Ported from BasicEnemyNavigationAgent.SetNewTarget in the original.
##
## How the wall/ceiling detours work: walking the navmesh path, now and then (chance_up spread
## over the whole path) we pick a random upward direction and cast a ray from the waypoint along
## it. If it hits a wall or the roof, that waypoint moves onto the surface (marked off_floor, with
## the surface normal). Following waypoints keep using the same direction while the surface stays
## at about the same distance (within `allowance`); when it changes too much the route drops back
## to the floor. So the creature sometimes scuttles along the ceiling for a stretch, then drops.
## The last waypoint always stays on the floor (the original could end the route on a wall up to
## 15 m from the goal and sit there).
##
## Mobile: one navmesh query and at most one ray per waypoint, only when the route is rebuilt
## (when the target moves a few metres), never per frame.
extends RefCounted


class Waypoint:
	var position: Vector3
	var normal := Vector3.UP
	var off_floor := false  ## on a wall or the ceiling

	func _init(at: Vector3) -> void:
		position = at


var chance_up := 0.3  ## chance per route of going up onto a wall or the ceiling
var up_check := 15.0  ## how far to look for a wall or ceiling (m)
var allowance := 0.25  ## how much the wall distance may change before dropping back (fraction)
var world_mask := 1

var points: Array[Waypoint] = []
var _wall_distance := 1.0  # distance to the surface when this stretch of wall started
var _rng := RandomNumberGenerator.new()


func _init() -> void:
	_rng.randomize()


func is_empty() -> bool:
	return points.is_empty()


func front() -> Waypoint:
	return points[0]


func last() -> Waypoint:
	return points[points.size() - 1]


## Re-plan from `from` to `to`. `on_wall`: the body is following a wall stretch right now.
func build(world: World3D, from: Vector3, to: Vector3, on_wall: bool) -> void:
	points.clear()
	var nav := NavigationServer3D.map_get_path(world.navigation_map, from, to, false)
	var space := world.direct_space_state
	var chance := chance_up / maxf(nav.size(), 1)
	var up := Vector3.UP
	for i in nav.size():
		var point := Waypoint.new(nav[i])
		if i == nav.size() - 1:  # always end on the floor, at the goal
			points.append(point)
			break
		if not on_wall:
			if _rng.randf() >= chance:
				points.append(point)
				continue
			up = Vector3(_rng.randf_range(-1, 1), _rng.randf(), _rng.randf_range(-1, 1)).normalized()
		on_wall = _lift(space, point, up, on_wall)
		points.append(point)


## Cast from the waypoint along `up`; if a surface is there (and, mid-stretch, about as far as
## where the stretch started) move the waypoint onto it. Returns whether we're on a wall after.
func _lift(space: PhysicsDirectSpaceState3D, point: Waypoint, up: Vector3, on_wall: bool) -> bool:
	var query := PhysicsRayQueryParameters3D.create(point.position, point.position + up * up_check, world_mask)
	var hit := space.intersect_ray(query)
	if hit.is_empty():
		return on_wall
	var distance := point.position.distance_to(hit.position)
	if on_wall and absf(distance - _wall_distance) > allowance * _wall_distance:
		return false
	if not on_wall:
		_wall_distance = maxf(distance, 0.01)
	point.position = hit.position
	point.normal = hit.normal
	point.off_floor = true
	return true
