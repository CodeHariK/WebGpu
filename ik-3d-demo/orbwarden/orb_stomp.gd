## The Orb Warden's stomp, in four beats:
##   rise     the body heaves up and back (nose up) while the two front-most walking legs lift
##            high and forward off the ground (RISE s) — the tell
##   slam     the feet drive down to the ground just ahead of where they stood and the body is
##            kicked down onto them (SLAM s, accelerating)
##   impact   `impact` fires at the point between the feet: the demo spawns the shockwave ring,
##            sparks and a camera shake
##   recover  the feet are handed back to the gait (they step home) and the body settles
## The legs are taken over with SpiderLeg's override (the same hook gestures use), so the gait
## and IK carry on untouched; the body moves through Spider's height / pitch offsets and spring.
class_name OrbStomp
extends Node

signal impact(at: Vector3)

enum Stage { IDLE, RISE, SLAM, HOLD, RECOVER }

const RISE := 0.8
const SLAM := 0.14
const HOLD := 0.35
const RECOVER := 0.7
const LIFT := 0.9 ## metres the body rises
const PITCH := 0.3 ## radians nose-up at the top
const RAISE := Vector3(0.0, 2.2, -1.6) ## front feet lifted to their rest spot + this (body space)
const REACH := 0.9 ## the slam lands this far ahead of the rest spot
const SLAM_KICK := Vector3(-7.0, -2.0, 0.0) ## body spring kick at the slam (down, nose down)

var warden: OrbWarden
var stage := Stage.IDLE

var _time := 0.0
var _legs: Array[int] = []
var _from: Array[Vector3] = []
var _top: Array[Vector3] = []
var _ground: Array[Vector3] = []


func busy() -> bool:
	return stage != Stage.IDLE


func status() -> String:
	return ["ready", "rising", "SLAM", "impact", "recovering"][stage]


## Begin a stomp with the two front-most legs that still walk (needs at least one).
func start() -> void:
	if busy():
		return
	var spider := warden.spider
	var walking: Array[int] = []
	for leg in spider.legs.size():
		if spider.rig.layout.legs[leg].walks:
			walking.append(leg)
	if walking.size() < 3: # too few to stand on while two are up
		return
	walking.sort_custom(func(a: int, b: int) -> bool: return spider.rig.layout.legs[a].hip.z < spider.rig.layout.legs[b].hip.z)
	_legs.clear()
	_legs.append(walking[0])
	_legs.append(walking[1])
	_from.clear()
	for leg in _legs:
		spider.legs[leg].begin_override()
		_from.append(spider.legs[leg].override_position)
	stage = Stage.RISE
	_time = 0.0


func update(delta: float) -> void:
	if stage == Stage.IDLE:
		return
	_time += delta
	var spider := warden.spider
	var body := spider.rig.skeleton.global_transform
	match stage:
		Stage.RISE:
			var e := smoothstep(0.0, 1.0, _time / RISE)
			_pose(LIFT * e, PITCH * e)
			_top.clear()
			for k in _legs.size():
				var leg := _legs[k]
				var raised := body * (spider.rig.layout.legs[leg].rest_foot() + RAISE)
				_top.append(raised)
				_place(k, _from[k].lerp(raised, e))
			if _time >= RISE:
				_ground.clear()
				for leg in _legs:
					var ahead := body * (spider.rig.layout.legs[leg].rest_foot() + Vector3.FORWARD * REACH)
					_ground.append(_ground_under(ahead))
				spider.kick_body(SLAM_KICK)
				_next(Stage.SLAM)
		Stage.SLAM:
			var t := clampf(_time / SLAM, 0.0, 1.0)
			_pose(lerpf(LIFT, 0.0, t), lerpf(PITCH, 0.0, t))
			for k in _legs.size():
				_place(k, _top[k].lerp(_ground[k], t * t))
			if _time >= SLAM:
				var center := Vector3.ZERO
				for point in _ground:
					center += point / float(_ground.size())
				impact.emit(center)
				_next(Stage.HOLD)
		Stage.HOLD:
			for k in _legs.size():
				_place(k, _ground[k])
			if _time >= HOLD:
				for leg in _legs:
					spider.legs[leg].release_override()
				_next(Stage.RECOVER)
		Stage.RECOVER:
			if _time >= RECOVER:
				_next(Stage.IDLE)


## Let go at once (e.g. a stomping leg broke off).
func cancel() -> void:
	if stage == Stage.IDLE:
		return
	for leg in _legs:
		if warden.spider.legs[leg].override:
			warden.spider.legs[leg].release_override()
	_pose(0.0, 0.0)
	stage = Stage.IDLE


func _next(next: Stage) -> void:
	stage = next
	_time = 0.0
	if next == Stage.IDLE:
		_pose(0.0, 0.0)


func _pose(height: float, pitch: float) -> void:
	warden.spider.body_height_offset = height
	warden.spider.body_pitch_offset = pitch


func _place(k: int, point: Vector3) -> void:
	var leg := warden.spider.legs[_legs[k]]
	if leg.override:
		leg.override_position = point


func _ground_under(point: Vector3) -> Vector3:
	var space := warden.spider.get_world_3d().direct_space_state
	var query := PhysicsRayQueryParameters3D.create(point + Vector3.UP * 6.0, point + Vector3.DOWN * 10.0, warden.spider.ground_mask)
	var hit := space.intersect_ray(query)
	return hit.position if not hit.is_empty() else Vector3(point.x, 0.0, point.z)
