## Slam (Z): both blades hook the enemy nearest the mouse (like a pull), then haul it up and smash it
## into the ground slams times — each time OUTSIDE the fov-wide cone in front of the way your body
## faces right now (to your sides or behind you), so every haul swings it up over your head and
## down hard on the far side, like a hammer — and let go after the last. Your body turns toward each
## landing (BodyPose: a yank turns it), so the cone turns with you and the next slam goes round again.
##
## Built on PullMove: every slam is one shared yank (both blades on one curve and one clock, side
## by side) along an arc over your head (_yank_path), slow off the ground and fastest at the impact
## ("in" easing); the longer the haul, the higher. Between slams the blades hold it in the ground
## for pause. Each landing: hit-stop, flash, a bigger shake, and any
## other enemy within splash is knocked away from it, and the enemy squashes flat for a moment.
class_name SlamMove
extends PullMove

var slams := 4
var pause := 0.1 ## seconds it stays smashed into the ground before the next haul
var radius_min := 2.5 ## m from you: where it can land …
var radius_max := 4.5
var fov := 2.094 ## radians (120°): … outside this cone round the body's facing …
const MIN_APART := 0.6 ## radians (≈35°): … and at least this far round from where it lies
var lift_min := 3.5 ## the arc's height (over your head) for a short haul …
var lift_max := 5.5 ## … and for the longest
var splash := 1.8 ## m: other enemies this close to a landing are knocked away

var left := 0 ## slams still to do


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "slam"


## The tuning panel's sliders for this move: [var name, min, max, step].
func tunables() -> Array:
	return [
		["slams", 1, 8, 1],
		["pause", 0, 0.5, 0.02],
		["radius_min", 1, 6, 0.1],
		["radius_max", 1, 8, 0.1],
		["fov", 0, 3.14, 0.05],
		["lift_min", 1, 8, 0.1],
		["lift_max", 1, 10, 0.1],
		["splash", 0, 5, 0.1],
	]


func reset() -> void:
	super()
	left = 0


func start() -> void:
	var enemy := demo.enemy_near_mouse(RANGE, true)
	if enemy >= 0 and demo.blades_free():
		left = slams
		fling = _pick_slam(enemy)
		throw_both(demo.enemies[enemy].position, enemy)


## A yank's end is a landing: smash, then haul it again (both blades hold it first) or let go.
func next_phase(blade: Blade) -> void:
	if blade.phase != Blade.YANK:
		super(blade) # OUT → hook + hold, HOLD → wait for both, then yank together
		return
	var index := blade.enemy
	var enemy := demo.enemies[index]
	enemy.yanked = false
	enemy.velocity = Vector3.ZERO
	demo.hit(blade, index, false)
	demo.shake = 0.45
	enemy.squash = 1.0
	_splash(index)
	left -= 1
	if left > 0:
		fling = _pick_slam(index)
		for other in _hooked_to(index): # pinned either side of it (the blade updated after this one
			var spot := enemy.position + demo.right() * other.side * BladesDemo.PAIR_GAP # still has last frame's tip)
			other.tip = spot
			other.hold(pause)
		return
	for other in demo.blades:
		if other.enemy == index:
			other.enemy = -1
			back(other)


## Slams hit harder: a longer freeze on every hit.
func hitstop_scale() -> float:
	return 1.8


## Up off the spot and in toward you, over your head, and straight down onto the landing.
func _yank_path(
		from: Vector3,
		to: Vector3,
		lift: float
) -> PackedVector3Array:
	var head := demo.player + Vector3.UP * BladesDemo.HAND_HEIGHT
	var p1 := from.lerp(head, 0.5) + Vector3.UP * lift
	var p2 := to.lerp(head, 0.25) + Vector3.UP * lift * 1.1
	return PackedVector3Array([from, p1, p2, to])


## The next slam: a random spot OUTSIDE the fov cone round the body's facing (as it is now), at
## least MIN_APART round from where enemy `index` lies; the longer the haul, the higher (and
## longer) the arc.
func _pick_slam(index: int) -> Dictionary:
	var facing := demo.body.pose.forward()
	var center := atan2(facing.x, facing.z)
	var at := demo.enemies[index].position - demo.player
	var lies := atan2(at.x, at.z)
	var angle := center + PI # fallback: straight behind
	for attempt in 8:
		var pick := center + fov * 0.5 + randf() * (TAU - fov)
		if absf(angle_difference(lies, pick)) >= MIN_APART:
			angle = pick
			break
	var to := Vector3(sin(angle), 0.0, cos(angle)) * randf_range(radius_min, radius_max)
	var haul := clampf((to - Vector3(at.x, 0.0, at.z)).length() / (radius_max * 2.0), 0.0, 1.0)
	return {
		"name": "slam %d/%d" % [slams - left + 1, slams],
		"to": to,
		"lift": lerpf(lift_min, lift_max, haul),
		"duration": lerpf(0.42, 0.55, haul),
		"carry": 0.0,
	}


## Knock every other (free) enemy near the landing away from it.
func _splash(index: int) -> void:
	var at := demo.enemies[index].position
	for i in demo.enemies.size():
		var other := demo.enemies[i]
		if i == index or other.bound > 0.0 or other.yanked:
			continue
		var away := other.position - at
		away.y = 0.0
		if away.length() < splash:
			other.velocity = (away.normalized() if away.length() > 1e-3 else Vector3.RIGHT) * 7.0 + Vector3.UP * 3.0
			other.flash = 1.0
