## The reaction to the player coming close (SpiderBrain starts it the moment the target crosses
## `alert_distance`): freeze dead still — low, turning slowly to face them — for a random beat,
## then rush straight at them, skid, and back off. The beat is random so the player can't time it.
##
##   freeze    no steps, body low, a slow turn to face the target; `beat_range` seconds
##   charge → recoil → backoff   as ChargeBehaviour, a little faster and closer
class_name AmbushBehaviour
extends ChargeBehaviour

var beat_range := Vector2(0.5, 1.2) ## seconds frozen before the rush
var freeze_turn := 1.5 ## turn strength while frozen: slow, deliberate

var _beat := 0.8


func _init() -> void:
	charge_speed = 3.2
	stop_distance = 1.0 # rushes right up to you


func label() -> String:
	return "ambush"


func _begin() -> void:
	set_phase("freeze")
	_beat = randf_range(beat_range.x, beat_range.y)


func _update(delta: float) -> void:
	if phase == "freeze":
		face_point(target.global_position, freeze_turn) # turn first…
		pose(-0.14) # …low and dead still
		if phase_time >= _beat:
			set_phase("charge") # …then go
		return
	super._update(delta)
