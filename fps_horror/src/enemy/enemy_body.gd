## The enemy's real body: a rolling ball (RigidBody3D sphere). Everything you see of the cultist
## later (step 7) is just limbs and a torso placed around this ball by IK; physics only ever moves
## the ball. Ported from BasicEnemyNavigationAgent in the original.
##
## This file does not push the ball itself. It works out where the ball should go:
##   route            an EnemyPath (navmesh, sometimes onto walls / the ceiling)
##   desired_velocity toward the next waypoint at max_speed; the limbs read it and kick the ball
##                    along it with impulses each time they step (limb_gait.gd)
##   grounded         NONE (in the air), FLOOR, or WALL_ROOF (stuck to a wall or the ceiling)
##   gravity          off while on a wall or ceiling, and the ball is pressed into the surface
##   launch_request   emitted when the ball should jump (onto or off a wall, or toward a wall
##                    waypoint from the floor); the gait answers with a big impulse
## Waypoints count as reached within stop_distance, or as soon as the next one is closer.
## If the ball gets no closer to its waypoint for stall_time, the route is re-planned along the
## floor (the original used a per-waypoint time limit that never fired).
##
## Mobile: the grounded test is one sphere overlap (+ one ray on walls) per physics tick, with the
## query built once (the original made a new shape every tick).
extends RigidBody3D

const EnemyPath := preload("res://src/enemy/enemy_path.gd")

enum Grounded { NONE, FLOOR, WALL_ROOF }

signal launch_request

@export var max_speed := 2.0  ## m/s, the speed the limbs aim for
@export var stop_distance := 3.0  ## a waypoint is reached this close
@export var ground_check := 1.25  ## anything within this radius of the centre counts as ground
@export var min_launch_delay := 0.25  ## at least this long on the ground between jumps
@export var stall_time := 2.5  ## no progress for this long: re-plan
@export var top_speed := 25.0  ## safety cap (m/s): launches stay well under it
@export_flags_3d_physics var world_mask := 1

var route := EnemyPath.new()
var goal := Vector3.ZERO  ## where we were asked to go
var target := Vector3.ZERO  ## where the route ends (on the navmesh, or a wall/ceiling spot near goal)
var desired_velocity := Vector3.ZERO
var grounded := Grounded.NONE
var ground_normal := Vector3.UP  ## the wall or ceiling normal while on one, else UP

var _waypoint: EnemyPath.Waypoint  # the one we're heading for (kept when the route runs out)
var _closest := INF  # nearest we've been to the current waypoint
var _stalled := 0.0
var _launch_wait := 0.0
var _gravity := 1.0
var _query := PhysicsShapeQueryParameters3D.new()


func _ready() -> void:
	_gravity = gravity_scale
	route.world_mask = world_mask
	_launch_wait = randf() * min_launch_delay
	var sphere := SphereShape3D.new()
	sphere.radius = ground_check
	_query.shape = sphere
	_query.collision_mask = world_mask
	_query.exclude = [get_rid()]
	goal = global_position
	target = global_position
	_waypoint = EnemyPath.Waypoint.new(global_position)


## Plan a new route to `to` (on the navmesh near it).
func set_target(to: Vector3) -> void:
	goal = to
	_plan(_waypoint.off_floor)


## `on_wall`: carry on along the wall we're on. A stuck re-plan starts from the floor instead (a
## wall stretch can lead somewhere the ball can't follow, e.g. under a low lintel).
func _plan(on_wall: bool) -> void:
	route.build(get_world_3d(), global_position, goal, on_wall)
	_stalled = 0.0
	target = route.last().position if not route.is_empty() else global_position
	_take_front()


func target_reached() -> bool:
	return route.is_empty()


## The waypoint the ball is heading for (the last one once the route is done).
func next_waypoint() -> EnemyPath.Waypoint:
	return _waypoint


func _physics_process(delta: float) -> void:
	if linear_velocity.length() > top_speed:
		linear_velocity = linear_velocity.limit_length(top_speed)
	_advance()
	grounded = _check_grounded()
	if route.is_empty():
		var away := target - global_position
		desired_velocity = away.normalized() * max_speed if away.length() > stop_distance else Vector3.ZERO
		_handle_gravity(_waypoint.off_floor)
		return
	if _stuck(delta):
		_plan(false)  # plan again, along the floor
		if route.is_empty():
			return
	if grounded == Grounded.NONE:
		_launch_wait = min_launch_delay
	else:
		_launch_wait = maxf(_launch_wait - delta, 0.0)
	_handle_gravity(_waypoint.off_floor)
	desired_velocity = (_waypoint.position - global_position).normalized() * max_speed


## Drop waypoints we've reached; jump when the route switches between floor and wall.
func _advance() -> void:
	if route.is_empty():
		return
	var points := route.points
	var closer_to_next := points.size() > 1 and \
		global_position.distance_to(points[0].position) > global_position.distance_to(points[1].position)
	var switch := points.size() > 1 and points[0].off_floor != points[1].off_floor
	if global_position.distance_to(points[0].position) <= stop_distance or closer_to_next:
		points.remove_at(0)
		_take_front()
		if switch:
			_launch_wait = min_launch_delay
			launch_request.emit()


func _take_front() -> void:
	if not route.is_empty():
		_waypoint = route.front()
		_closest = INF


## True once the ball has gone stall_time without getting 0.5 m closer to its waypoint.
func _stuck(delta: float) -> bool:
	var distance := global_position.distance_to(_waypoint.position)
	if distance < _closest - 0.5:
		_closest = distance
		_stalled = 0.0
		return false
	_stalled += delta
	return _stalled > stall_time


## Heading for a wall waypoint: on the wall, no gravity and press into it; on the floor, jump.
func _handle_gravity(waypoint_off_floor: bool) -> void:
	if not waypoint_off_floor:
		gravity_scale = _gravity
		return
	if grounded == Grounded.WALL_ROOF:
		if linear_velocity.dot(-ground_normal) < 3.0:  # press in, but don't build up speed
			linear_velocity -= ground_normal * 0.5
		gravity_scale = 0.0
		return
	if grounded == Grounded.FLOOR and _launch_wait <= 0.0:
		_launch_wait = min_launch_delay
		launch_request.emit()
	gravity_scale = _gravity


func _check_grounded() -> Grounded:
	ground_normal = Vector3.UP
	_query.transform = global_transform
	var space := get_world_3d().direct_space_state
	if space.intersect_shape(_query, 1).is_empty():
		return Grounded.NONE
	if _waypoint.normal == Vector3.UP:
		return Grounded.FLOOR
	var ray := PhysicsRayQueryParameters3D.create(global_position, global_position - _waypoint.normal * ground_check, world_mask)
	var hit := space.intersect_ray(ray)
	if hit.is_empty():
		return Grounded.FLOOR
	ground_normal = hit.normal
	return Grounded.WALL_ROOF
