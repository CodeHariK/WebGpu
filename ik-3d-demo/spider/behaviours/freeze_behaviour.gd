## Dead still, slowly turning to watch the target, breathing. Sometimes ends with a twitch.
class_name FreezeBehaviour
extends SpiderBehaviour

var duration_range := Vector2(0.8, 2.2)
var twitch_time := 0.15

var _duration := 1.0


func label() -> String:
	return "freeze"


func _begin() -> void:
	set_phase("freeze")
	_duration = randf_range(duration_range.x, duration_range.y)


func _update(_delta: float) -> void:
	match phase:
		"freeze":
			face_point(target.global_position, 1.0)
			pose(-0.06 + sin(phase_time * 2.2) * 0.015) # low, breathing
			if phase_time >= _duration:
				if randf() < 0.5:
					set_phase("twitch")
				else:
					finish()
		"twitch":
			pose(0.04, 0.1, 0.08 * sin(phase_time * 50.0))
			if phase_time >= twitch_time:
				finish()
