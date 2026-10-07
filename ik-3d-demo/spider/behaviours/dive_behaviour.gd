## Kamikaze: leap at the target with every leg stretched out, and explode on contact.
##
##   approach  if too far to leap, scuttle in fast until `leap_range`
##   windup    face it, sink low and tremble harder and harder (the tell — give the player a
##             moment to react)
##   air       Spider.launch(), aimed to land right on the target; legs snap out straight and
##             swept forward (Spider.air_splay), nose down
##   boom      on reaching the target, or touching down: SpiderExplosion, spider hidden
##   respawn   after `respawn_time`, a new spider drops in from above at `respawn_distance`,
##             legs reaching for the ground, and squashes on landing
class_name DiveBehaviour
extends SpiderBehaviour

var leap_range := 6.5 ## leaps from at most this far
var approach_speed := 1.6 ## × move_speed
var max_approach_time := 3.0
var windup_time := 0.7
var up_speed := 4.0 ## m/s at launch: low and fast (flight time 2 · up / gravity)
var splay_time := 0.12 ## legs snap out over this long after take-off
var hit_radius := 0.9 ## explodes when the body gets this close to the target
var power := 1.0 ## explosion size
var respawn_time := 2.5
var respawn_distance := 6.0
var drop_height := 3.0
var land_time := 0.35

var _flight_velocity: Vector3 ## flat, m/s


func label() -> String:
	return "dive"


func _begin() -> void:
	set_phase("approach" if distance() > leap_range else "windup")


## Cut short (brain switched off or another behaviour forced): never leave the spider hidden.
func interrupt() -> void:
	spider.visible = true


func _update(_delta: float) -> void:
	match phase:
		"approach":
			face_point(target.global_position, 6.0)
			drive(toward() * approach_speed)
			if distance() <= leap_range or phase_time >= max_approach_time:
				set_phase("windup")
		"windup":
			face_point(target.global_position, 10.0)
			var t := minf(phase_time / windup_time, 1.0)
			var shake := sin(phase_time * 70.0) * 0.06 * t # trembling builds up
			pose(-0.3 * t, 0.2 * t + shake, shake)
			if phase_time >= windup_time:
				_leap()
		"air":
			face_point(target.global_position, 8.0)
			drive(_flight_velocity / spider.move_speed)
			spider.air_splay = minf(phase_time / splay_time, 1.0)
			pose(0.0, -0.4)
			var body := spider.rig.skeleton.global_position
			if body.distance_to(target.global_position) <= hit_radius or not spider.airborne:
				_explode()
		"boom":
			if phase_time >= respawn_time:
				_respawn()
		"drop":
			face_point(target.global_position, 3.0)
			if not spider.airborne:
				set_phase("land")
		"land":
			var squash := 1.0 - phase_time / land_time
			pose(-0.3 * squash, -0.1 * squash)
			if phase_time >= land_time:
				finish()


# Aim to come down right on the target: flat distance / flight time.
func _leap() -> void:
	var flight_time := 2.0 * up_speed / spider.jump_gravity
	_flight_velocity = to_target() / flight_time
	spider.launch(up_speed)
	set_phase("air")


func _explode() -> void:
	SpiderExplosion.spawn(spider, power)
	spider.visible = false
	set_phase("boom")


# A fresh spider drops in somewhere around the target, facing it.
func _respawn() -> void:
	var angle := randf() * TAU
	var spot := target.global_position * Vector3(1, 0, 1) + Vector3(cos(angle), 0.0, sin(angle)) * respawn_distance
	var facing := spot.direction_to(target.global_position * Vector3(1, 0, 1))
	spider.teleport(spot + Vector3.UP * drop_height, atan2(-facing.x, -facing.z))
	spider.visible = true
	spider.launch(0.0) # falls from here, legs reaching down for the ground
	set_phase("drop")
