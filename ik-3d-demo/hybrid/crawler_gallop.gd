## The crawler's fast gait: big bounding leaps. Not realistic on purpose: it springs high off all
## fours, hangs for a moment, then drops hard under extra gravity and is off again, like a
## horror-film thing coming at you. The cultist's speed and hop, but each leap is planned, so
## the limbs land on real footholds instead of being dragged.
##
## Why a different gait: the walking gait only steps a limb once it lags `step_distance` behind,
## and a planted hand must hold on while the body passes over it. At 6 m/s the body would pass a
## whole arm's reach in 0.15 s. So instead it leaps: short stances, long flights.
##
## One leap:
##   stance   all four limbs down for `stance_time`; the body crouches `crouch` and springs up
##   take-off every limb lets go and folds to its relaxed pose (crawler_gait.hang_limb), so
##            nothing is stretched out in the air
##   flight   the body rises `hop_height` above its ride height and falls back under
##            `hop_gravity` (much more than real gravity: up quick, a short hang, down hard)
##   landing  each limb reaches for the ground `reach_ahead` seconds before it touches (front
##            pair first, hind pair `stagger` later), aimed where its rest spot will be half-way
##            through the next stance, through the same foothold search as walking
## The height is visual (added to the posed body after its springs, so they can't soften the
## drop); the walker underneath keeps moving at run speed, on floors, walls or ceilings alike.
## The body pitches with its vertical speed: nose up rising, nose down diving (`rock`).
## `ride_offset` raises (or lowers) the whole gallop above the walking height.
##
## It switches on above start_speed and off below 80 % of it (so it doesn't flicker); `amount`
## fades the leaps in and out over a third of a second.
class_name CrawlerGallop
extends RefCounted

enum Phase { STANCE, FLIGHT }

var start_speed := 2.6 ## gallops above this (m/s)
var stance_time := 0.14 ## s with all limbs down between leaps
var crouch := 0.08 ## m the body sinks during a stance
var hop_height := 0.45 ## m the body leaps above its ride height
var hop_gravity := 45.0 ## m/s² pulling the leap down (real gravity is 9.8)
var reach_ahead := 0.16 ## s before touch-down a limb starts reaching for the ground
var stagger := 0.04 ## s the hind pair lands after the front pair
var ride_offset := 0.12 ## m added to the ride height while galloping
var lift := 0.12 ## m: how high the landing arcs lift off the surface
var rock := 0.4 ## radians of pitch at full vertical speed

var active := false
var amount := 0.0 ## 0..1 how much of the leap to show
var phase := Phase.STANCE
var height := 0.0 ## m above ride height right now (negative while crouching)
var vertical_speed := 0.0 ## m/s

var _time := 0.0
var _reaching: Array[bool] = [false, false, false, false]


## Should it be galloping at `speed`? (with hysteresis)
func wants(speed: float) -> bool:
	var was := active
	active = speed > start_speed or (active and speed > start_speed * 0.8)
	if active and not was:
		_land()
	return active


## Fade the leaps in or out.
func blend(delta: float) -> void:
	amount = move_toward(amount, 1.0 if active else 0.0, delta * 3.0)
	if not active:
		height = move_toward(height, 0.0, delta * 2.0)
		vertical_speed = 0.0


func in_flight() -> bool:
	return active and phase == Phase.FLIGHT


## Run the leap: crouch, take off, fly, reach for the ground, land.
func update(gait: CrawlerGait, space: PhysicsDirectSpaceState3D, delta: float, frame: Transform3D,
		velocity: Vector3, body: Transform3D) -> void:
	_time += delta
	if phase == Phase.STANCE:
		height = -crouch * sin(PI * clampf(_time / stance_time, 0.0, 1.0))
		if _time >= stance_time:
			_take_off(gait)
		return
	vertical_speed -= hop_gravity * delta
	height += vertical_speed * delta
	var left := _time_to_land()
	for limb in CrawlerLayout.COUNT:
		var touch := left + (stagger if CrawlerLayout.is_foot(limb) else 0.0)
		if not _reaching[limb] and touch <= reach_ahead:
			var home := frame * CrawlerLayout.rest(limb) + velocity * (touch + stance_time * 0.5)
			# judge reach from where the body will be when it lands, not from up in the air
			var landing := body
			landing.origin += velocity * touch - body.basis.y * (height + ride_offset) * amount
			gait.step_limb_to(space, frame, limb, landing, home, maxf(touch, 0.03), lift)
			_reaching[limb] = true
	if height <= 0.0 and vertical_speed < 0.0:
		_land()


## (height, pitch) to add to the posed body this frame, already faded by `amount`.
func body_motion() -> Vector2:
	var top := sqrt(2.0 * hop_gravity * maxf(hop_height, 0.001))
	var pitch := rock * clampf(vertical_speed / top, -1.0, 1.0) if phase == Phase.FLIGHT else 0.0
	return Vector2(ride_offset + height, pitch) * amount


func _take_off(gait: CrawlerGait) -> void:
	phase = Phase.FLIGHT
	_time = 0.0
	height = 0.0
	vertical_speed = sqrt(2.0 * hop_gravity * maxf(hop_height, 0.001))
	for limb in CrawlerLayout.COUNT:
		gait.hang_limb(limb)
		_reaching[limb] = false


func _land() -> void:
	phase = Phase.STANCE
	_time = 0.0
	height = 0.0
	vertical_speed = 0.0


## Seconds until the leap is back down to ride height.
func _time_to_land() -> float:
	var v := vertical_speed
	return (v + sqrt(v * v + 2.0 * hop_gravity * maxf(height, 0.0))) / hop_gravity
