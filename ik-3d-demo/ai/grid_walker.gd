## Walks a Spider along TacticalGrid paths to a target: re-plans every REPLAN seconds (or when the
## target has moved), then drives the spider through Spider's steering hooks — the gait does the
## legs. Waypoints reached by WALK are walked to. DROP: it walks off the ledge toward the spot and,
## once there's no ground under it, falls (Spider.launch with no lift). LADDER and JUMP waypoints
## are leapt to from the waypoint before: a ballistic launch timed to land on the spot (spiders
## leap ladders). After any landing it re-plans from wherever it came down.
class_name GridWalker
extends Node

const REPLAN := 0.5 ## seconds between re-plans
const REACHED := 0.45 ## metres (flat) from a walk waypoint that counts as there
const ARRIVE := 0.9 ## stop this far from the target
const TURN_RATE := 5.0 ## rad/s per radian off: how hard it turns to face where it's going
const LEAP_CLEARANCE := 0.7 ## a leap peaks this far above the higher end
const FALL_GAP := 0.6 ## walking off a ledge: once the ground is this far below, it falls

var spider: Spider
var grid: TacticalGrid
var target: Node3D
var path: Array[Dictionary] = []
var status := "idle" ## idle | walking | leaping | arrived | no path
var waypoint := 0 ## index of the path waypoint being headed for

var _replan_left := 0.0
var _planned_for := Vector3.INF
var _flight := Vector3.ZERO ## flat m/s while leaping


func _physics_process(delta: float) -> void:
	if spider == null or grid == null or target == null:
		return
	spider.steering = true
	if spider.airborne:
		_steer_flat(_flight / spider.move_speed, _flight)
		return
	if status == "leaping": # just landed (maybe not quite where planned): plan again from here
		status = "walking"
		_replan_left = 0.0
	_replan_left -= delta
	if _replan_left <= 0.0 or target.global_position.distance_to(_planned_for) > 0.75:
		_plan()
	_follow()


func _plan() -> void:
	_replan_left = REPLAN
	_planned_for = target.global_position
	path = grid.find_path(spider.global_position, target.global_position)
	waypoint = 0
	status = "walking" if not path.is_empty() else "no path"


func _follow() -> void:
	var to_target := target.global_position - spider.global_position
	if Vector2(to_target.x, to_target.z).length() < ARRIVE and absf(to_target.y) < 1.5:
		status = "arrived"
		_steer_flat(Vector3.ZERO, to_target)
		return
	if waypoint >= path.size():
		_steer_flat(Vector3.ZERO, to_target)
		return
	var next: Dictionary = path[waypoint]
	var offset: Vector3 = next.position - spider.global_position
	var flat := Vector3(offset.x, 0.0, offset.z)
	if next.kind == TacticalGrid.Link.DROP:
		_walk_off(next.position, flat)
		return
	if next.kind != TacticalGrid.Link.WALK:
		_leap_to(next.position)
		return
	if flat.length() < REACHED and absf(offset.y) < 1.0:
		waypoint += 1
		return
	_steer_flat(flat.normalized(), flat)


# Walk toward the spot below the ledge; when the ground under the spider falls away, drop.
func _walk_off(landing: Vector3, flat: Vector3) -> void:
	_steer_flat(flat.normalized(), flat)
	var ground = spider.ground_ray.get("hit")
	if ground != null and spider.global_position.y - ground.y > FALL_GAP:
		_flight = flat.normalized() * spider.move_speed * 0.8
		spider.launch(0.0)
		status = "leaping"


# A ballistic leap onto `landing`: up fast enough to clear the higher end, across at the speed
# that gets there in the flight time.
func _leap_to(landing: Vector3) -> void:
	var gravity := spider.jump_gravity
	var rise := landing.y - spider.global_position.y
	var up_speed := sqrt(2.0 * gravity * (maxf(rise, 0.0) + LEAP_CLEARANCE))
	var flight_time := (up_speed + sqrt(maxf(up_speed * up_speed - 2.0 * gravity * rise, 0.0))) / gravity
	var flat := landing - spider.global_position
	flat.y = 0.0
	_flight = flat / maxf(flight_time, 0.05)
	spider.launch(up_speed)
	status = "leaping"


# Drive along `velocity` (in units of move_speed, world space) while turning to face `facing`.
func _steer_flat(velocity: Vector3, facing: Vector3) -> void:
	var local := spider.global_basis.inverse() * velocity
	spider.steer = Vector2(local.x, -local.z)
	var flat := Vector2(facing.x, facing.z)
	if flat.length() < 0.05:
		spider.steer_turn = 0.0
		return
	var forward := -spider.global_basis.z
	var off := Vector2(forward.x, forward.z).angle_to(flat)
	spider.steer_turn = clampf(-off * TURN_RATE, -4.0, 4.0)
