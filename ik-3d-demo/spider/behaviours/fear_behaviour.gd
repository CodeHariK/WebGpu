## Spooked: flinch, bolt away low and fast, then stop, turn round and peek back, trembling.
##
##   flinch  jerk back and up for a moment
##   flee    run directly away (a crab scuttles sideways) until `safe_distance`
##   peek    skid to a stop, turn to face the target again, crouched and shivering
class_name FearBehaviour
extends SpiderBehaviour

var flinch_time := 0.2
var flee_speed := 2.2 ## × move_speed
var safe_distance := 7.0
var max_flee_time := 2.5
var skid_time := 0.35 ## the run slows to a stop over this long as it turns to peek
var peek_time := 1.4


func label() -> String:
	return "fear"


func _begin() -> void:
	set_phase("flinch")


func _update(_delta: float) -> void:
	match phase:
		"flinch":
			face_point(target.global_position)
			drive(-toward() * 0.6)
			pose(0.1, 0.35)
			if phase_time >= flinch_time:
				set_phase("flee")
		"flee":
			face_travel(-toward(), 6.0)
			drive(-toward() * flee_speed)
			pose(-0.15, -0.1)
			if distance() >= safe_distance or phase_time >= max_flee_time:
				set_phase("peek")
		"peek":
			var skid := 1.0 - minf(phase_time / skid_time, 1.0) # bleed off the run's speed
			drive(-toward() * flee_speed * skid * skid)
			face_point(target.global_position, 2.5)
			var shiver := sin(phase_time * 70.0) * 0.025
			pose(-0.18 + shiver, 0.05, shiver)
			if phase_time >= peek_time:
				finish()
