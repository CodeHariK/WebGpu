## Random walk: amble to random spots around the target, pausing at each — minding its own
## business, but never wandering off the stage.
class_name WanderBehaviour
extends SpiderBehaviour

var area_radius := 6.0 ## picks spots within this distance of the target
var min_spot_distance := 1.5 ## (and at least this far from the target)
var speed := 0.7 ## × move_speed
var arrive_distance := 0.4
var pause_range := Vector2(0.2, 0.8)
var duration_range := Vector2(4.0, 8.0)

var _spot: Vector3
var _pause := 0.0
var _duration := 5.0
var _elapsed := 0.0


func label() -> String:
	return "wander"


func _begin() -> void:
	_duration = randf_range(duration_range.x, duration_range.y)
	_elapsed = 0.0
	_pick_spot()


func _update(delta: float) -> void:
	_elapsed += delta
	if _elapsed >= _duration:
		finish()
		return
	match phase:
		"walk":
			var offset := _spot - spider.global_position
			offset.y = 0.0
			if offset.length() <= arrive_distance or phase_time > 6.0:
				set_phase("pause")
				_pause = randf_range(pause_range.x, pause_range.y)
				return
			face_travel(offset.normalized(), 3.0)
			drive(offset.normalized() * speed)
		"pause":
			if phase_time >= _pause:
				_pick_spot()


func _pick_spot() -> void:
	var angle := randf() * TAU
	var reach := randf_range(min_spot_distance, area_radius)
	_spot = target.global_position + Vector3(cos(angle), 0.0, sin(angle)) * reach
	set_phase("walk")
