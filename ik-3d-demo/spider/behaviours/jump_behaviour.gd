## Hop in close to the target and land beside it.
##
##   approach  walk in until `jump_range` (skipped if already close)
##   crouch    stop, face it, sink down — the tell before the jump
##   air       Spider.launch(); fly toward a spot `land_offset` to one side of the target, facing it.
##             Horizontal speed = distance / flight time, so it lands right there.
##   land      squash on touchdown, then spring back up
class_name JumpBehaviour
extends SpiderBehaviour

var jump_range := 3.2 ## walks closer first if further than this
var approach_speed := 1.2 ## × move_speed
var max_approach_time := 2.5
var crouch_time := 0.35
var up_speed := 5.5 ## m/s at launch
var land_offset := 1.3 ## lands this far from the target, to its left or right
var land_time := 0.4

var _landing_spot: Vector3
var _flight_velocity: Vector3 ## flat, m/s


func label() -> String:
	return "jump"


func _begin() -> void:
	set_phase("approach" if distance() > jump_range else "crouch")


func _update(_delta: float) -> void:
	match phase:
		"approach":
			face_travel(toward())
			drive(toward() * approach_speed)
			if distance() <= jump_range or phase_time >= max_approach_time:
				set_phase("crouch")
		"crouch":
			face_point(target.global_position, 8.0)
			pose(-0.22 * minf(phase_time / crouch_time, 1.0), 0.1)
			if phase_time >= crouch_time:
				_take_off()
		"air":
			face_point(target.global_position, 6.0)
			drive(_flight_velocity / spider.move_speed)
			pose(0.0, -0.15) # nose dips toward the landing
			if not spider.airborne:
				set_phase("land")
		"land":
			face_point(target.global_position)
			var squash := 1.0 - phase_time / land_time
			pose(-0.25 * squash, -0.1 * squash)
			if phase_time >= land_time:
				finish()


func _take_off() -> void:
	var side := Vector3.UP.cross(toward()) * (1.0 if randf() < 0.5 else -1.0)
	_landing_spot = target.global_position + side * land_offset
	var flat := _landing_spot - spider.global_position
	flat.y = 0.0
	var flight_time := 2.0 * up_speed / spider.jump_gravity
	_flight_velocity = flat / flight_time
	spider.launch(up_speed)
	set_phase("air")
