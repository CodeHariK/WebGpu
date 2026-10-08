## What one creature knows about the player: sight, hearing, a suspicion meter and a memory.
##
## Sight: the player is sampled at a few points (feet, chest, head). Each point inside the
## creature's view cone and range gets a ray from the eye; the share that gets through is
## `visible_fraction` (behind a low wall only the head shows: 1/3).
##
## Suspicion (0..1) — detection isn't instant:
##   fills while the player is visible, faster when close, in the middle of the view, more of the
##   player visible, and when the player moves; drains slowly when not
##   the player right in its face (< point_blank) → alert at once
## Awareness, from the meter and the memory:
##   UNAWARE     nothing yet                       SUSPICIOUS  saw or heard something: look, check it
##   ALERT       knows where the player is         SEARCHING   lost the player: hunt the last known
##                                                             position, then give up slowly
## Hearing: hear() with a noise's position and loudness (radius). Heard noises raise suspicion and
## give a point to check; while alert or searching they also update the last known position.
## Memory: last_known (+ velocity, time): where the player was last seen or heard.
## Squad: share() is a squadmate's report — alert, with the reported position.
class_name CreatureSenses
extends RefCounted

enum Awareness { UNAWARE, SUSPICIOUS, SEARCHING, ALERT }

var view_angle := deg_to_rad(120.0) ## the whole cone
var view_range := 14.0
var point_blank := 2.5 ## seeing the player this close = alert at once
var fill_rate := 1.4 ## suspicion per second at best (close, centred, fully visible)
var moving_bonus := 1.6 ## × fill when the player moves faster than 0.5 m/s
var drain_rate := 0.15 ## suspicion lost per second while nothing is seen
var suspicious_at := 0.2
var lose_after := 1.2 ## alert, unseen this long → searching
var search_time := 10.0 ## searching this long without a sighting → back to unaware
var collision_mask := 1

var awareness := Awareness.UNAWARE
var suspicion := 0.0
var sees_player := false
var visible_fraction := 0.0
var last_known := Vector3.INF ## INF = never seen or heard
var last_known_velocity := Vector3.ZERO
var interest := Vector3.INF ## the point a suspicious creature should look at / check
var unseen_for := 0.0 ## seconds since the player was last seen
var unheard_for := INF ## seconds since the player was last heard
var search_left := 0.0

var eye := Vector3.ZERO ## last update's eye and facing (for debug drawing)
var forward := Vector3.FORWARD


## Look for the player. `points` = where on the player to check (feet, chest, head…).
func update(delta: float, space: PhysicsDirectSpaceState3D, from_eye: Vector3, facing: Vector3, points: Array[Vector3], player_velocity: Vector3) -> void:
	eye = from_eye
	forward = facing.normalized() if facing.length() > 0.01 else forward
	visible_fraction = _visible_fraction(space, points)
	sees_player = visible_fraction > 0.0
	var centre := _average(points)
	if sees_player:
		unseen_for = 0.0
		last_known = centre
		last_known_velocity = player_velocity
		interest = centre
		suspicion = minf(suspicion + _fill(centre, player_velocity) * delta, 1.0)
		if eye.distance_to(centre) < point_blank:
			suspicion = 1.0
	else:
		unseen_for += delta
		unheard_for += delta
		if awareness != Awareness.ALERT:
			suspicion = maxf(suspicion - drain_rate * delta, 0.0)
	_update_awareness(delta)


## A noise at `position` audible within `radius` metres. Louder (closer) = more suspicious.
func hear(position: Vector3, radius: float) -> void:
	var distance := eye.distance_to(position)
	if distance > radius:
		return
	var strength := 1.0 - distance / radius
	unheard_for = 0.0
	suspicion = maxf(suspicion, minf(0.35 + 0.4 * strength, 0.95)) # a noise alone never fully alerts
	interest = position
	if awareness == Awareness.ALERT or awareness == Awareness.SEARCHING:
		last_known = position # it knows it's you: go there
		search_left = search_time
	elif awareness == Awareness.UNAWARE:
		awareness = Awareness.SUSPICIOUS


## A squadmate's report (radio): the player is at `position`. Alerts this creature and keeps its
## memory fresh as if it saw the player itself.
func share(position: Vector3, velocity: Vector3) -> void:
	awareness = Awareness.ALERT
	suspicion = 1.0
	last_known = position
	last_known_velocity = velocity
	interest = position
	unseen_for = 0.0


func _update_awareness(delta: float) -> void:
	match awareness:
		Awareness.ALERT:
			if unseen_for > lose_after:
				awareness = Awareness.SEARCHING
				search_left = search_time
		Awareness.SEARCHING:
			search_left -= delta
			if sees_player and suspicion >= 0.6: # it's looking for you: recognises you fast
				awareness = Awareness.ALERT
			elif search_left <= 0.0:
				awareness = Awareness.UNAWARE
				suspicion = suspicious_at * 0.5
		_:
			if suspicion >= 1.0:
				awareness = Awareness.ALERT
			elif suspicion >= suspicious_at:
				awareness = Awareness.SUSPICIOUS
			else:
				awareness = Awareness.UNAWARE


# Share of the player's points inside the cone and range with a clear line from the eye.
func _visible_fraction(space: PhysicsDirectSpaceState3D, points: Array[Vector3]) -> float:
	if points.is_empty():
		return 0.0
	var seen := 0
	for point in points:
		var to_point := point - eye
		if to_point.length() > view_range or forward.angle_to(to_point) > view_angle * 0.5:
			continue
		if space.intersect_ray(PhysicsRayQueryParameters3D.create(eye, point, collision_mask)).is_empty():
			seen += 1
	return float(seen) / points.size()


# Suspicion per second for a player at `centre`.
func _fill(centre: Vector3, player_velocity: Vector3) -> float:
	var to_player := centre - eye
	var near := 1.0 - clampf(to_player.length() / view_range, 0.0, 1.0)
	var centred := lerpf(0.4, 1.0, cos(forward.angle_to(to_player)) * 0.5 + 0.5)
	var moving := moving_bonus if player_velocity.length() > 0.5 else 1.0
	var boost := 3.0 if awareness == Awareness.SEARCHING else 1.0 # it's looking for you
	return fill_rate * visible_fraction * (0.25 + near) * centred * moving * boost


static func _average(points: Array[Vector3]) -> Vector3:
	var sum := Vector3.ZERO
	for point in points:
		sum += point
	return sum / maxf(points.size(), 1.0)
