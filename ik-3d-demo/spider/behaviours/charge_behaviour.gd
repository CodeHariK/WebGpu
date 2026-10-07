## Fast charge at the target, then back off a little — a bluff, not an attack.
##
##   windup    stop, face it, crouch and rock back (telegraphs the charge to the player)
##   charge    straight at it, fast, nose down, until `stop_distance` (or out of time)
##   recoil    skid to a stop (speed bleeds off), nose up
##   backoff   walk backwards, still facing it, out to `backoff_distance`
class_name ChargeBehaviour
extends SpiderBehaviour

var windup_time := 0.45
var charge_speed := 2.8 ## × move_speed
var stop_distance := 1.5 ## metres from the target where the charge ends (the skid carries it ~0.4 m closer)
var min_distance := 0.8 ## the skid never carries it closer than this (it doesn't run into the target)
var max_charge_time := 2.0
var recoil_time := 0.3
var backoff_speed := 0.55 ## × move_speed
var backoff_distance := 2.6 ## stops backing off this far away (or after max_backoff_time)
var max_backoff_time := 1.8


func label() -> String:
	return "charge"


func _begin() -> void:
	set_phase("windup")


func _update(_delta: float) -> void:
	match phase:
		"windup":
			face_point(target.global_position, 8.0)
			var shake := sin(phase_time * 60.0) * 0.03 # trembling with excitement
			pose(-0.12, 0.12 + shake, shake)
			if phase_time >= windup_time:
				set_phase("charge")
		"charge":
			face_point(target.global_position, 8.0)
			drive(toward() * charge_speed)
			pose(-0.08, -0.15)
			if distance() <= stop_distance or phase_time >= max_charge_time:
				set_phase("recoil")
		"recoil": # skid: bleed the speed off instead of stopping dead (feet keep up)
			face_point(target.global_position)
			var skid := 1.0 - minf(phase_time / recoil_time, 1.0)
			if distance() > min_distance:
				drive(toward() * charge_speed * skid * skid)
			pose(0.05, 0.25)
			if phase_time >= recoil_time:
				set_phase("backoff")
		"backoff":
			face_point(target.global_position)
			drive(-toward() * backoff_speed)
			pose(0.0, 0.08)
			if distance() >= backoff_distance or phase_time >= max_backoff_time:
				finish()
