## Whirl (X): enemy as a weapon. Both blades hook the enemy nearest the mouse; it is swung round you
## on the chains — a lead-in curve onto a circle, then quarter-circle Beziers, faster and faster —
## knocking down anyone it touches, and let go when the swing points at the mouse: it flies off as
## a projectile, still bowling over anyone it hits until it lands.
class_name WhirlMove
extends BladeMove

const RANGE := 12.0
const RADIUS := 3.6
const TURNS := 3
const TIME := 1.5
const MIN_TURNS := 1.5 ## release no earlier than this many turns …
const AIM := 0.96 ## … when (swing direction · aim direction) is above this
const THROW_SPEED := 18.0
const BODY_HIT := 1.2 ## a swung / thrown enemy knocks over others this close

var held := -1 ## the enemy being swung
var _angle := 0.0 ## where on the circle the swing starts
var _from := Vector3.ZERO ## where the enemy was when hooked (the lead-in's start)
var _right := Vector3.RIGHT ## the circle's frame, fixed when the swing starts
var _forward := Vector3.FORWARD


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "whirl"


func reset() -> void:
	held = -1


func start() -> void:
	if not demo.both_idle():
		return
	var enemy := demo.enemy_near_mouse(RANGE, true)
	if enemy >= 0:
		throw_both(demo.enemies[enemy].position, enemy)


## Hooked: the first blade to arrive starts the swing, the second joins it in step.
func next_phase(blade: Blade) -> void:
	if blade.phase != Blade.OUT:
		super(blade)
		return
	if blade.enemy < 0:
		return
	demo.hit(blade, blade.enemy, false)
	if held < 0:
		_start_whirl(blade)
	elif blade.phase == Blade.OUT:
		blade.start(Blade.WHIRL, whirl_path(), TIME, "accelerate")
		blade.time = demo.blades[0].time if blade.side > 0.0 else demo.blades[1].time


## The circle starts where the enemy already is (its angle round the player), in a frame fixed
## now, so turning to aim doesn't spin the circle.
func _start_whirl(caller: Blade) -> void:
	var enemy := caller.enemy
	held = enemy
	_right = demo.right()
	_forward = demo.facing
	var offset := demo.enemies[enemy].position - demo.player
	_angle = atan2(offset.dot(_forward), offset.dot(_right))
	_from = demo.enemies[enemy].position
	demo.enemies[enemy].yanked = true # the path moves it now
	start_together(demo.blades, caller, Blade.WHIRL, whirl_path(), TIME, "accelerate") # one clock: one swing


## Lead-in (from the enemy to the circle, arriving along its tangent) + TURNS turns of
## quarter-circle Beziers round the player.
func whirl_path() -> PackedVector3Array:
	var center := demo.player + Vector3(0, 1.4, 0)
	var k := Bezier.QUARTER_CIRCLE * RADIUS
	var start_point := _circle(center, _angle)
	var path := PackedVector3Array([_from, _from.lerp(start_point, 0.4) + Vector3.UP * 0.8, start_point - _tangent(_angle) * k, start_point])
	for q in TURNS * 4:
		var a0 := _angle + q * PI / 2.0
		var a1 := a0 + PI / 2.0
		var p0 := _circle(center, a0)
		var p3 := _circle(center, a1)
		path.append_array([p0, p0 + _tangent(a0) * k, p3 - _tangent(a1) * k, p3])
	return path


func live_path(blade: Blade) -> PackedVector3Array:
	return whirl_path() if blade.phase == Blade.WHIRL else PackedVector3Array()


## The enemy rides the path; one blade (the right) decides when to let go.
func after_tip(blade: Blade) -> bool:
	if blade.phase != Blade.WHIRL or blade.enemy < 0:
		return false
	demo.enemies[blade.enemy].position = blade.tip
	var swing := Bezier.path_tangent(blade.path, blade.eased).normalized()
	blade.tip += demo.right() * blade.side * BladesDemo.PAIR_GAP
	if blade.side < 0.0 or held < 0:
		return false
	var turns := blade.eased * (TURNS * 4 + 1) / 4.0 - 0.25 # the lead-in is a quarter
	var aim := demo.mouse_point() - demo.player
	aim.y = 0.0
	if (turns >= MIN_TURNS and swing.dot(aim.normalized()) > AIM) or blade.time >= blade.duration:
		_release(swing)
		return true
	return false


## Let go along the swing's direction right now: the enemy becomes a projectile.
func _release(direction: Vector3) -> void:
	var enemy := demo.enemies[held]
	enemy.yanked = false
	enemy.thrown = true
	var flat := Vector3(direction.x, 0.0, direction.z).normalized()
	enemy.velocity = flat * THROW_SPEED + Vector3.UP * 5.0
	held = -1
	demo.shake = 0.25
	for blade in demo.blades:
		blade.enemy = -1
		back(blade)


## A swung or thrown enemy bowls over any other enemy it touches.
func tick(_delta: float) -> void:
	var enemies := demo.enemies
	for i in enemies.size():
		var weapon := enemies[i]
		if not (weapon.thrown or i == held):
			continue
		for j in enemies.size():
			var other := enemies[j]
			if j == i or other.cooldown > 0.0 or other.bound > 0.0 or other.yanked:
				continue
			if weapon.position.distance_to(other.position) > BODY_HIT:
				continue
			var away := other.position - weapon.position
			away.y = 0.0
			other.velocity = away.normalized() * 9.0 + Vector3.UP * 4.0
			other.flash = 1.0
			other.cooldown = 0.4
			demo.fire.spark((weapon.position + other.position) * 0.5)
			demo.hitstop = BladesDemo.HITSTOP * 0.7
			demo.shake = maxf(demo.shake, 0.2)


func _circle(
		center: Vector3,
		angle: float
) -> Vector3:
	return center + (_right * cos(angle) + _forward * sin(angle)) * RADIUS


func _tangent(angle: float) -> Vector3:
	return -_right * sin(angle) + _forward * cos(angle)
