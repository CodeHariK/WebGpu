## Jump smash (Space, hammers): you leap at the enemy nearest the mouse on a high arc, both heads
## swinging up over you, and come down on it: the heads smash onto its head together and flatten it.
## You bounce back up off it — press Space again while you're in the air to leap straight on to the
## next one (the one under the mouse, else the nearest other), as many times as you like;
## otherwise you fall and land.
class_name LeapMove
extends HammerMove

var leap_time := 0.6 ## seconds for the leap
var height := 2.4 ## m the arc rises above its higher end
var hover := 1.4 ## m above the enemy's head your feet are at the smash
var stand_off := 0.9 ## m short of it (in front)
var reach := 12.0 ## m: the farthest enemy you leap at
var bounce := 8.0 ## m/s you bounce up off it …
var bounce_back := 3.0 ## … and back

var carrying := false ## the leap is moving you (BladesDemo leaves the player to it)
var combo := 0 ## smashes in a row
var _from := Vector3.ZERO
var _to := Vector3.ZERO
var _time := 0.0
var _enemy := -1
var _last := -1 ## the enemy just smashed (−1: none)
var _toward := Vector3.FORWARD


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "leap"


func tunables() -> Array:
	return [
		["leap_time", 0.2, 1.5, 0.01], ["height", 0.5, 6.0, 0.05], ["hover", 0.3, 3.0, 0.05], ["stand_off", 0.0, 2.5, 0.05],
		["reach", 3.0, 20.0, 0.5], ["bounce", 0.0, 16.0, 0.1], ["bounce_back", 0.0, 8.0, 0.1],
	]


func reset() -> void:
	carrying = false
	_last = -1
	combo = 0


## In the air, bouncing off a head you just smashed (a press now leaps on to the next one).
func bouncing() -> bool:
	return _last >= 0 and not carrying and demo.player.y > 0.05


## Leap at the enemy nearest the mouse — or, bouncing off a head, on to the next one.
func start() -> void:
	var enemy := demo.enemy_near_mouse(reach, false)
	if bouncing():
		if enemy < 0 or enemy == _last:
			enemy = _nearest_other()
	elif carrying or not demo.blades_free() or demo.player.y > 0.05:
		return
	else:
		combo = 0
	if enemy < 0:
		return
	_leap(enemy)


func hits_during(_phase: int) -> bool:
	return false # the smash does it


func knocks_back() -> bool:
	return false


## The leap carries you along its arc.
func tick(delta: float) -> void:
	if not carrying:
		if demo.player.y <= 0.05 and demo.player_velocity == Vector3.ZERO:
			_last = -1 # landed: the chain is over
		return
	_time += delta
	var u := clampf(_time / leap_time, 0.0, 1.0)
	var a := _from + Vector3.UP * height
	var b := _to + Vector3.UP * height * 0.6
	demo.player = Bezier.point(_from, a, b, _to, u)
	demo.facing = _toward


func next_phase(blade: Blade) -> void:
	if blade.phase == Blade.OUT:
		if blade == demo.blades[1]:
			_smash()
		rebound(blade, 0.4) # off its head, up with you as you bounce
	else:
		super(blade)


func _leap(enemy: int) -> void:
	_enemy = enemy
	carrying = true
	_time = 0.0
	var target := demo.enemies[enemy].position
	var flat := Vector3(target.x - demo.player.x, 0.0, target.z - demo.player.z)
	_toward = flat.normalized() if flat.length() > 0.1 else demo.facing
	var head := demo.enemies[enemy].head_top()
	_from = demo.player
	_to = head + Vector3.UP * hover - _toward * stand_off
	demo.player_velocity = Vector3.ZERO
	var apex := _from.lerp(_to, 0.5) + Vector3.UP * (height + BladesDemo.HAND_HEIGHT + 1.6)
	for blade in demo.blades:
		begin(blade, enemy)
		blade.delay = 0.0
		var side := demo.right() * blade.side * 0.3
		var over := apex - _toward * 0.8 + side
		var down := head + side
		var from := blade.tip
		var path := PackedVector3Array([from, from + Vector3.UP * 1.5 - _toward * 0.5, over - _toward * 1.0 - Vector3.UP * 0.6, over])
		path.append_array([over, over + _toward * 1.5 + Vector3.UP * 0.5, down + Vector3.UP * 2.0, down])
		blade.start_even(Blade.OUT, path, leap_time, "accelerate")


# The heads land on its head: flatten it, and you bounce back up off it.
func _smash() -> void:
	var enemy := demo.enemies[_enemy]
	enemy.squash = 1.0
	enemy.flash = 1.0
	enemy.velocity = Vector3.ZERO
	demo.fire.spark(enemy.head_top())
	demo.impacts.impact(enemy.head_top(), 14, 30, 6.0, Color(0.3, 0.32, 0.35)) # scrap off its head
	demo.shake = maxf(demo.shake, 0.55)
	demo.hitstop = demo.hitstop_time * 2.5
	carrying = false
	_last = _enemy
	combo += 1
	demo.player_velocity = -_toward * bounce_back + Vector3.UP * bounce


# The nearest enemy other than the one just smashed, within reach.
func _nearest_other() -> int:
	var best := -1
	var best_distance := reach
	for i in demo.enemies.size():
		var d := demo.enemies[i].position.distance_to(demo.player)
		if i != _last and d < best_distance:
			best = i
			best_distance = d
	return best
