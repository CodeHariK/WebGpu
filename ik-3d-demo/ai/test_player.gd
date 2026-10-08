## The stand-in player for the AI demos: an orb that walks where you click.
##   Click        walk there (stopped by walls, steps up stairs, falls off edges; footsteps are
##                NoiseBus noises that carry FOOTSTEP_RADIUS)
##   Shift-click  put it there at once (it can't climb onto a platform by walking)
##   Right-click  look there (turns `facing` without moving)
## The orb's centre floats 0.5 above the ground; the eye is EYE_HEIGHT above the centre.
class_name TestPlayer
extends Node

const SPEED := 2.2 ## m/s
const EYE_HEIGHT := 1.1
const FOOTSTEP_EVERY := 0.45 ## seconds
const FOOTSTEP_RADIUS := 3.0

var orb: Node3D
var goal := Vector3.INF ## where it's walking to
var facing := Vector3(-1, 0, 1).normalized() ## flat
var _footstep_left := 0.0


func eye() -> Vector3:
	return orb.global_position + Vector3.UP * EYE_HEIGHT


## Handle a mouse click (from the demo's _unhandled_input). True if it was used.
func handle_click(event: InputEvent, camera: Camera3D) -> bool:
	var click := event as InputEventMouseButton
	if click == null or not click.pressed:
		return false
	if click.button_index != MOUSE_BUTTON_LEFT and click.button_index != MOUSE_BUTTON_RIGHT:
		return false
	var from := camera.project_ray_origin(click.position)
	var to := from + camera.project_ray_normal(click.position) * 200.0
	var hit := orb.get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(from, to))
	if hit.is_empty():
		return true
	var flat: Vector3 = hit.position - orb.global_position
	flat.y = 0.0
	if click.button_index == MOUSE_BUTTON_RIGHT or click.shift_pressed:
		if flat.length() > 0.1:
			facing = flat.normalized()
	if click.button_index == MOUSE_BUTTON_LEFT:
		if click.shift_pressed:
			orb.global_position = hit.position + Vector3.UP * 0.5
			goal = Vector3.INF
		else:
			goal = hit.position
	return true


func _process(delta: float) -> void:
	if orb == null or goal == Vector3.INF:
		return
	var flat := goal - orb.global_position
	flat.y = 0.0
	if flat.length() < 0.05:
		goal = Vector3.INF
		return
	var space := orb.get_world_3d().direct_space_state
	var step := flat.normalized() * minf(SPEED * delta, flat.length())
	var next := orb.global_position + step
	if not space.intersect_ray(PhysicsRayQueryParameters3D.create(orb.global_position, next + step.normalized() * 0.2)).is_empty():
		goal = Vector3.INF # walked into a wall
		return
	var ground := space.intersect_ray(PhysicsRayQueryParameters3D.create(next + Vector3.UP * 0.6, next + Vector3.DOWN * 4.0))
	if not ground.is_empty():
		next.y = ground.position.y + 0.5
	orb.global_position = next
	facing = flat.normalized()
	_footstep_left -= delta
	if _footstep_left <= 0.0:
		_footstep_left = FOOTSTEP_EVERY
		NoiseBus.emit(orb.global_position + Vector3.DOWN * 0.4, FOOTSTEP_RADIUS)
