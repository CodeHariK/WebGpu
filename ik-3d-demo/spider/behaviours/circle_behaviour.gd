## Circle the target while facing it — sideways, like a crab sizing you up. Sometimes stops and
## goes back the other way.
##
##   inward = direction to the target        around = inward × up (counter-clockwise)
##   move   = around · direction · speed  +  inward · (distance − radius) · hold_strength
##   turn   = feed-forward (the turn rate a circle needs) + correction toward the target
class_name CircleBehaviour
extends SpiderBehaviour

var radius_range := Vector2(2.5, 5.0) ## circles at its current distance, clamped to this
var speed := 1.0 ## along the circle, × move_speed
var hold_strength := 1.5 ## distance correction per metre off
var duration_range := Vector2(3.0, 6.0)
var reverse_chance := 0.5 ## chance of one change of direction partway round

var _radius := 3.0
var _duration := 4.0
var _direction := 1.0 ## +1 counter-clockwise from above, -1 clockwise
var _smoothed := 1.0 ## eases through 0 on a reversal: slow down, pause, go back
var _reverse_at := -1.0


func label() -> String:
	return "circle"


func _begin() -> void:
	set_phase("circle")
	_radius = clampf(distance(), radius_range.x, radius_range.y)
	_duration = randf_range(duration_range.x, duration_range.y)
	_direction = 1.0 if randf() < 0.5 else -1.0
	_smoothed = 0.0
	_reverse_at = randf_range(0.3, 0.7) * _duration if randf() < reverse_chance else -1.0


func _update(delta: float) -> void:
	if _reverse_at > 0.0 and phase_time >= _reverse_at:
		_direction = -_direction
		_reverse_at = -1.0
	_smoothed = move_toward(_smoothed, _direction, delta * 2.5)
	var current := distance()
	if current < 0.01:
		finish()
		return
	var inward := toward()
	var radial := clampf((current - _radius) * hold_strength, -1.0, 1.0)
	drive(inward.cross(Vector3.UP) * _smoothed * speed + inward * radial)
	face_point(target.global_position)
	spider.steer_turn += _smoothed * speed * spider.move_speed * spider.speed_scale / current
	if phase_time >= _duration:
		finish()
