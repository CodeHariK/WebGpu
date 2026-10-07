## Stand and shoot: plant, turn on the target, wait for the head to line up, then fire bursts
## from the head gun (SpiderGun). The recoil knocks the head off aim; firing pauses when it's
## too far off and resumes as the springs bring it back.
##
##   aim    stop, face the target, brace (crouch); until the gun is on target (or out of time)
##   fire   bursts of `burst_time` with `pause_time` gaps, only while roughly on target
##   cool   a moment of stillness after the last burst
## Only for creatures whose head has a gun (available()).
class_name ShootBehaviour
extends SpiderBehaviour

var gun: SpiderGun
var aim_tolerance := 0.12 ## radians: starts firing once the gun points this close to the target
var fire_tolerance := 0.35 ## radians: keeps firing while within this (recoil walks it off)
var max_aim_time := 1.0
var burst_time := 0.45
var pause_time := 0.3
var duration_range := Vector2(1.8, 3.0) ## seconds of firing
var cool_time := 0.4

var _duration := 2.0


func label() -> String:
	return "shoot"


## Shooting needs a gun on the head.
func available() -> bool:
	return gun != null and gun.has_gun()


func _begin() -> void:
	set_phase("aim")
	_duration = randf_range(duration_range.x, duration_range.y)


func _update(_delta: float) -> void:
	face_point(target.global_position, 6.0)
	pose(-0.05, -0.04) # braced, leaning into it
	match phase:
		"aim":
			if gun.aim_error(target.global_position) < aim_tolerance or phase_time >= max_aim_time:
				set_phase("fire")
		"fire":
			var in_burst := fmod(phase_time, burst_time + pause_time) < burst_time
			if in_burst and gun.aim_error(target.global_position) < fire_tolerance:
				gun.trigger = true
			if phase_time >= _duration:
				set_phase("cool")
		"cool":
			if phase_time >= cool_time:
				finish()
