## Rush / grapple (Space): both blades hook the enemy or post ring nearest the mouse on screen,
## then YOU travel a lifted arc to it (ease-in: a beat of tension, then violent), the chains
## shortening as you go. An enemy: you land in front of it with a slam. A post: you fly up to its
## ring, keep your momentum and fall (BladesDemo moves the player in the air).
class_name RushMove
extends BladeMove

const POSTS := [Vector3(-7, 4.5, -9), Vector3(7, 5.5, -11), Vector3(0, 6.5, -16)] ## grapple rings (tops)
const PICK_PIXELS := 90.0 ## the target: the enemy or ring nearest the mouse on screen, this close
var stop := 1.3 ## an enemy rush ends this far in front of it

var rushing := false
var _path := PackedVector3Array() ## the player's feet: 4 control points
var _time := 0.0
var _duration := 0.0
var _enemy := -1 ## -1: a post
var _target := Vector3.ZERO


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "rush"


## The tuning panel's sliders for this move: [var name, min, max, step].
func tunables() -> Array:
	return [
		["stop", 0.5, 3, 0.1],
	]


func reset() -> void:
	rushing = false


func start() -> void:
	if not demo.blades_free() or rushing:
		return
	var target := Vector3.ZERO
	var enemy := -2
	var candidates: Array[Vector3] = []
	for e in demo.enemies:
		candidates.append(e.position)
	candidates.append_array(POSTS)
	var best := _pick(candidates)
	if best >= 0:
		target = candidates[best]
		enemy = best if best < demo.enemies.size() else -1
	if enemy == -2:
		return
	_enemy = enemy
	_target = target
	throw_both(target, enemy)


# Which of `points` you aim at: nearest the mouse on screen (within PICK_PIXELS) — or, in a
# preview (an aim point instead of the mouse, while you move the editor camera), nearest that
# point (within BladesDemo.PICK_RADIUS); −1: none.
func _pick(points: Array[Vector3]) -> int:
	var best := -1
	if demo.aim_override != Vector3.INF:
		var best_distance := BladesDemo.PICK_RADIUS
		for i in points.size():
			var d := points[i].distance_to(demo.aim_override)
			if d < best_distance:
				best = i
				best_distance = d
		return best
	var camera := demo.camera
	var best_pixels := PICK_PIXELS
	for i in points.size():
		if camera.is_position_behind(points[i]):
			continue
		var pixels := camera.unproject_position(points[i]).distance_to(demo.mouse)
		if pixels < best_pixels:
			best = i
			best_pixels = pixels
	return best


## Hooked: stay in the target while the player travels to it.
func next_phase(blade: Blade) -> void:
	if blade.phase == Blade.OUT:
		if blade.enemy >= 0:
			demo.hit(blade, blade.enemy, false)
		blade.hold(99.0)
		if not rushing:
			_start_rush()
	else:
		super(blade)


## The player's arc (feet): up and over, down onto the spot — in front of the enemy, or just under
## the ring.
func _start_rush() -> void:
	var from := demo.player
	var to: Vector3
	if _enemy >= 0:
		var foot := demo.enemies[_enemy].position
		foot.y = 0.0
		var flat := foot - Vector3(from.x, 0.0, from.z)
		to = foot - flat.normalized() * stop if flat.length() > stop else from
	else:
		to = _target + Vector3(0, -BladesDemo.HAND_HEIGHT - 0.4, 0) # hands just under the ring
	var distance := from.distance_to(to)
	var lift := 1.5 + distance * 0.12
	_path = PackedVector3Array([from, from + Vector3.UP * lift, to + Vector3.UP * lift * 0.6, to])
	_duration = 0.25 + distance * 0.025
	_time = 0.0
	rushing = true
	demo.player_velocity = Vector3.ZERO


func tick(delta: float) -> void:
	if not rushing:
		return
	_time += delta
	var t := clampf(_time / _duration, 0.0, 1.0)
	var p := _path
	var previous := demo.player
	demo.player = Bezier.point(p[0], p[1], p[2], p[3], Blade.eased_time(t, "in"))
	var flat := Vector3(p[3].x - demo.player.x, 0.0, p[3].z - demo.player.z)
	if flat.length() > 0.3:
		demo.facing = flat.normalized()
	if t < 1.0:
		return
	rushing = false
	if _enemy >= 0: # land on it: slam
		demo.hit(demo.blades[0], _enemy, true)
		demo.hitstop = demo.hitstop_time * 2.0
		demo.shake = 0.4
	else: # the ring: keep the speed of the arrival and fly on, then fall
		demo.player_velocity = (demo.player - previous) / maxf(delta, 1e-4) * 0.5
	for blade in demo.blades:
		if blade.phase == Blade.HOLD or blade.phase == Blade.OUT:
			back(blade)


func draw_debug(debug: BladeDebug) -> void:
	if rushing:
		debug.curve_lines(_path, Color(0.4, 1.0, 0.6))
