## Base for one thing the spider can decide to do (charge, jump, circle, flee, freeze, wander).
## SpiderBrain picks one, calls start(), then tick() every physics frame until `done`.
##
## Before every tick the brain clears the spider's steering hooks, so a behaviour only writes
## what it wants this frame — nothing it set earlier lingers:
##   drive(world_move)      move along a flat world vector, in units of spider.move_speed
##   face_point / face_travel / face_direction   turn (steer_turn)
##   pose(height, pitch, roll)                   crouch, rear, recoil, squash (action_pose)
## A behaviour is a little phase machine: set_phase() + phase_time.
class_name SpiderBehaviour
extends RefCounted

var spider: Spider
var target: Node3D ## the "player" it reacts to
var done := false
var phase := ""
var phase_time := 0.0 ## seconds since the phase started


## Short name for the HUD / debugging.
func label() -> String:
	return "behaviour"


## Whether this creature can do it at all (e.g. shooting needs a gun). The brain never picks or
## forces an unavailable behaviour.
func available() -> bool:
	return true


func start() -> void:
	done = false
	_begin()


func tick(delta: float) -> void:
	phase_time += delta
	_update(delta)


## Called when this behaviour is cut short before finishing (another one forced, brain off).
## Undo anything that must not linger (e.g. a hidden spider). Steering hooks need nothing:
## the brain clears those every frame.
func interrupt() -> void:
	pass


# --- override these ---

func _begin() -> void:
	pass


func _update(_delta: float) -> void:
	pass


# --- helpers ---------------------------------------------------------------------------------

func set_phase(new_phase: String) -> void:
	phase = new_phase
	phase_time = 0.0


func finish() -> void:
	done = true


## Flat vector from the spider to the target.
func to_target() -> Vector3:
	var offset := target.global_position - spider.global_position
	offset.y = 0.0
	return offset


func distance() -> float:
	return to_target().length()


## Flat unit direction toward the target.
func toward() -> Vector3:
	return to_target().normalized()


## Move along `world_move` (flat, world space) in units of move_speed: length 1 = walking pace.
func drive(world_move: Vector3) -> void:
	var local := spider.global_basis.inverse() * world_move
	spider.steer = Vector2(local.x, -local.z)


## Turn so the front points along `direction` (flat, world space). `strength` = rad/s per rad off.
func face_direction(direction: Vector3, strength := 4.0) -> void:
	if direction.length_squared() < 0.0001:
		return
	var local := spider.global_basis.inverse() * direction
	var angle_off := atan2(-local.x, -local.z) # 0 = dead ahead (forward is -Z)
	spider.steer_turn = clampf(angle_off * strength, -6.0, 6.0)


func face_point(point: Vector3, strength := 4.0) -> void:
	var offset := point - spider.global_position
	offset.y = 0.0
	face_direction(offset, strength)


## Face the way it's going. A sideways walker (crab) instead turns side-on to the travel
## direction — right side or left side leading, whichever needs less turning — and scuttles.
func face_travel(direction: Vector3, strength := 4.0) -> void:
	if not spider.rig.layout.sideways:
		face_direction(direction, strength)
		return
	var right_leads := Vector3.UP.cross(direction) # the forward that puts +X (right) along `direction`
	var forward := -spider.global_basis.z
	face_direction(right_leads if right_leads.dot(forward) >= 0.0 else -right_leads, strength)


func pose(height: float, pitch := 0.0, roll := 0.0) -> void:
	spider.action_pose = Vector3(height, pitch, roll)
