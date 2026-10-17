## Slam (Z): both blades hook the enemy nearest the mouse (like a pull), then haul it up and smash it
## into the ground SLAMS times — each time OUTSIDE the FOV-wide cone in front of the way your body
## faces right now (to your sides or behind you), so every haul swings it up over your head and
## down hard on the far side, like a hammer — and let go after the last. Your body turns toward each
## landing (BodyPose: a yank turns it), so the cone turns with you and the next slam goes round again.
##
## Built on PullMove: every slam is one shared yank (both blades on one curve and one clock, side
## by side) along an arc over your head (_yank_path), slow off the ground and fastest at the impact
## ("in" easing); the longer the haul, the higher. Between slams the blades hold it in the ground
## for PAUSE. Each landing: hit-stop, flash, a bigger shake, and any
## other enemy within SPLASH is knocked away from it, and the enemy squashes flat for a moment.
class_name SlamMove
extends PullMove

const SLAMS := 4
const PAUSE := 0.1 ## seconds it stays smashed into the ground before the next haul
const RADIUS_MIN := 2.5 ## m from you: where it can land …
const RADIUS_MAX := 4.5
const FOV := 2.094 ## radians (120°): … outside this cone round the body's facing …
const MIN_APART := 0.6 ## radians (≈35°): … and at least this far round from where it lies
const LIFT_MIN := 3.5 ## the arc's height (over your head) for a short haul …
const LIFT_MAX := 5.5 ## … and for the longest
const SPLASH := 1.8 ## m: other enemies this close to a landing are knocked away

var left := 0 ## slams still to do


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "slam"


func reset() -> void:
	super()
	left = 0


func start() -> void:
	var enemy := demo.enemy_near_mouse(RANGE, true)
	if enemy >= 0 and demo.blades_free():
		left = SLAMS
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
			other.hold(PAUSE)
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


## The next slam: a random spot OUTSIDE the FOV cone round the body's facing (as it is now), at
## least MIN_APART round from where enemy `index` lies; the longer the haul, the higher (and
## longer) the arc.
func _pick_slam(index: int) -> Dictionary:
	var facing := demo.body.pose.forward()
	var center := atan2(facing.x, facing.z)
	var at := demo.enemies[index].position - demo.player
	var lies := atan2(at.x, at.z)
	var angle := center + PI # fallback: straight behind
	for attempt in 8:
		var pick := center + FOV * 0.5 + randf() * (TAU - FOV)
		if absf(angle_difference(lies, pick)) >= MIN_APART:
			angle = pick
			break
	var to := Vector3(sin(angle), 0.0, cos(angle)) * randf_range(RADIUS_MIN, RADIUS_MAX)
	var haul := clampf((to - Vector3(at.x, 0.0, at.z)).length() / (RADIUS_MAX * 2.0), 0.0, 1.0)
	return {
		"name": "slam %d/%d" % [SLAMS - left + 1, SLAMS],
		"to": to,
		"lift": lerpf(LIFT_MIN, LIFT_MAX, haul),
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
		if away.length() < SPLASH:
			other.velocity = (away.normalized() if away.length() > 1e-3 else Vector3.RIGHT) * 7.0 + Vector3.UP * 3.0
			other.flash = 1.0
