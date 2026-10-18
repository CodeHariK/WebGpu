## Lasso (H): ONE chain (the right blade) hooks the enemy nearest the mouse and whirls it round in a
## circle above your head, for as long as you like. Aim with the mouse: the enemy nearest it glows.
## Press H again to throw: it lets go the next time the swing points at that enemy (or at the mouse
## point if none is near), on a ballistic arc that lands right on it — bowling over whoever it hits
## (thrown enemies are handled by WhirlMove.tick).
##
## The circle: quarter-circle Beziers in a horizontal ring height above your feet, rebuilt every
## frame round where you are (so it moves with you). The first lap starts with a lead-in from where
## the enemy was hooked onto the ring; after that each lap is one ring, played at constant speed
## (Blade.start_even), and the next lap starts when one ends.
class_name LassoMove
extends BladeMove

const RANGE := 12.0
var radius := 2.0 ## m: the ring round your head
var height := 3.3 ## m above your feet
var lap_time := 0.55 ## seconds per lap
var lead_time := 0.3 ## seconds from the hook onto the ring
const AIM := 0.92 ## let go when the swing direction · the aim direction is above this
var throw_speed := 16.0 ## m/s across the ground

var held := -1 ## the enemy on the chain
var throwing := false ## H pressed again: let go at the next good moment
var _from := Vector3.ZERO ## where it was hooked (the lead-in's start)
var _angle := 0.0 ## where on the ring each lap starts (world angle round you, atan2(x, z))
var _lead := false ## the lap playing now has the lead-in in front of it


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "lasso"


## The tuning panel's sliders for this move: [var name, min, max, step].
func tunables() -> Array:
	return [
		["radius", 1, 4, 0.1],
		["height", 2, 5, 0.1],
		["lap_time", 0.2, 1.5, 0.05],
		["lead_time", 0.1, 1, 0.05],
		["throw_speed", 5, 30, 0.5],
	]


func reset() -> void:
	held = -1
	throwing = false


## H: hook (both blades idle), or, while swinging, throw.
func start() -> void:
	if held >= 0:
		throwing = true
		return
	if not demo.blades_free():
		return
	var enemy := demo.enemy_near_mouse(RANGE, true)
	if enemy < 0:
		return
	var blade := demo.blades[1]
	begin(blade, enemy)
	blade.delay = 0.0
	var hand := demo.hand(blade.side)
	var target := demo.enemies[enemy].position
	var distance := hand.distance_to(target)
	launch(blade, PackedVector3Array([hand, hand, target + swing_up(blade) * distance * demo.drop, target]), demo.throw_time + distance * demo.throw_per_m, "swing")


func next_phase(blade: Blade) -> void:
	match blade.phase:
		Blade.OUT:
			if blade.enemy < 0:
				blade.hold(0.06)
				return
			demo.hit(blade, blade.enemy, false)
			held = blade.enemy
			throwing = false
			demo.enemies[held].yanked = true # the ring carries it now
			_from = demo.enemies[held].position
			var offset := _from - demo.player
			_angle = atan2(offset.x, offset.z)
			_lead = true
			blade.start_even(Blade.LASSO, lap_path(), lead_time + lap_time, "linear")
		Blade.LASSO: # a lap is done: another one
			_lead = false
			blade.start_even(Blade.LASSO, lap_path(), lap_time, "linear")
		_:
			super(blade)


## One lap of the ring round your head (4 quarter-circle Beziers from _angle), with the lead-in
## from the hook spot in front of it on the first lap.
func lap_path() -> PackedVector3Array:
	var center := demo.player + Vector3.UP * height
	var k := Bezier.QUARTER_CIRCLE * radius
	var start_point := _ring(center, _angle)
	var path := PackedVector3Array()
	if _lead:
		path.append_array([_from, _from.lerp(start_point, 0.4) + Vector3.UP * 1.0, start_point - _tangent(_angle) * k, start_point])
	for q in 4:
		var a0 := _angle + q * PI / 2.0
		var a1 := a0 + PI / 2.0
		var p0 := _ring(center, a0)
		var p3 := _ring(center, a1)
		path.append_array([p0, p0 + _tangent(a0) * k, p3 - _tangent(a1) * k, p3])
	return path


func live_path(blade: Blade) -> PackedVector3Array:
	return lap_path() if blade.phase == Blade.LASSO else PackedVector3Array()


## The enemy rides the chain's end; once you've asked to throw, it lets go when the swing points at
## the target (never during the lead-in).
func after_tip(blade: Blade) -> bool:
	if blade.phase != Blade.LASSO or held < 0:
		return false
	demo.enemies[held].position = blade.tip
	if not throwing or (_lead and blade.eased < 0.2): # the lead-in is the first of 5 segments
		return false
	var target := aim_point()
	var swing := Bezier.path_tangent(blade.path, blade.eased)
	swing.y = 0.0
	var to := target - blade.tip
	to.y = 0.0
	if swing.length() > 1e-3 and to.length() > 1e-3 and swing.normalized().dot(to.normalized()) > AIM:
		_release(blade, target)
		return true
	return false


## Where the throw goes: the enemy nearest the mouse (not the one on the chain), else the mouse point.
func aim_point() -> Vector3:
	var enemy := aim_enemy()
	return demo.enemies[enemy].position if enemy >= 0 else demo.mouse_point()


## The enemy nearest the mouse point (within BladesDemo.PICK_RADIUS), not the one on the chain; −1: none.
func aim_enemy() -> int:
	var aim := demo.mouse_point()
	var best := -1
	var best_distance := BladesDemo.PICK_RADIUS
	for i in demo.enemies.size():
		if i == held:
			continue
		var d := demo.enemies[i].position.distance_to(aim)
		if d < best_distance:
			best = i
			best_distance = d
	return best


## While swinging, the target glows a little.
func tick(_delta: float) -> void:
	if held < 0:
		return
	var enemy := aim_enemy()
	if enemy >= 0:
		demo.enemies[enemy].flash = maxf(demo.enemies[enemy].flash, 0.35)


func draw_debug(debug: BladeDebug) -> void:
	if held >= 0:
		debug.line(demo.enemies[held].position, aim_point(), Color(1.0, 0.4, 0.2))


func hits_during(phase: int) -> bool:
	return phase == Blade.OUT


# Let go: a ballistic arc from where it is now that comes down on `target` (horizontal speed
# throw_speed, gravity DemoEnemy.GRAVITY), as a thrown enemy that bowls over whoever it touches.
func _release(
		blade: Blade,
		target: Vector3
) -> void:
	var enemy := demo.enemies[held]
	enemy.yanked = false
	enemy.thrown = true
	var to := target - enemy.position
	var flat := Vector3(to.x, 0.0, to.z)
	var time := maxf(flat.length() / throw_speed, 0.25)
	enemy.velocity = flat / time + Vector3.UP * (to.y + 0.5 * DemoEnemy.GRAVITY * time * time) / time
	held = -1
	throwing = false
	demo.shake = 0.25
	blade.enemy = -1
	back(blade)


func _ring(
		center: Vector3,
		angle: float
) -> Vector3:
	return center + Vector3(sin(angle), 0.0, cos(angle)) * radius


func _tangent(angle: float) -> Vector3:
	return Vector3(cos(angle), 0.0, -sin(angle))
