## Pull (E): both blades hook the enemy nearest the mouse, hit-stop, then YANK it along a lifted
## arc (ease-in: slow, then violent) to a random spot: your feet, flung off to your left or right,
## or over your head to land behind you. The first blade in waits for the second; then both yank
## together on one curve, side by side, and let go together when it lands.
class_name PullMove
extends BladeMove

const RANGE := 12.0

var fling := {} ## where the yanked enemy goes {name, to, lift, duration, carry}, picked once per pull


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "pull"


func reset() -> void:
	fling = {}


func start() -> void:
	var enemy := demo.enemy_near_mouse(RANGE, true)
	if enemy >= 0 and demo.blades_free():
		fling = pick_fling()
		throw_both(demo.enemies[enemy].position, enemy)


## Where a pull throws the enemy. Over the head (behind) needs a higher arc and a bit more time;
## the side and back throws keep some speed so the enemy tumbles on after landing.
func pick_fling() -> Dictionary:
	var right := demo.right()
	var forward := demo.facing
	var choices := [
		{"name": "at your feet", "to": forward * 1.6, "lift": 2.0, "duration": 0.32, "carry": 0.0},
		{"name": "flung left", "to": -right * 4.5 + forward * 0.5, "lift": 2.5, "duration": 0.36, "carry": 5.0},
		{"name": "flung right", "to": right * 4.5 + forward * 0.5, "lift": 2.5, "duration": 0.36, "carry": 5.0},
		{"name": "over your head", "to": -forward * 5.0, "lift": 5.0, "duration": 0.48, "carry": 4.0},
	]
	return choices[randi() % choices.size()]


## The blades hooked into enemy `index` (both, in a pull).
func _hooked_to(index: int) -> Array[Blade]:
	var hooked: Array[Blade] = []
	for blade in demo.blades:
		if blade.enemy == index and blade.move == key:
			hooked.append(blade)
	return hooked


## One yank for every blade in the enemy: the same curve and the same clock, so the enemy flies
## as one piece and neither blade lets go mid-air (each still sits PAIR_GAP to its own side).
func _yank_together(
		caller: Blade,
		hooked: Array[Blade]
) -> void:
	var index := caller.enemy
	var from := demo.enemies[index].position
	var to: Vector3 = demo.player + fling.to + Vector3(0, DemoEnemy.STAND_HEIGHT, 0)
	var lift: float = fling.lift
	demo.enemies[index].yanked = true
	start_together(hooked, caller, Blade.YANK, _yank_path(from, to, lift), fling.duration, "in")


## The yank's curve from `from` to `to`: straight up off the spot, over, and down onto it.
func _yank_path(
		from: Vector3,
		to: Vector3,
		lift: float
) -> PackedVector3Array:
	return PackedVector3Array([from, from + Vector3.UP * lift, to + Vector3.UP * lift * 0.8, to])


func fling_name() -> String:
	return fling.get("name", "")


## While yanking, the enemy rides the path; the blades sit either side of it.
func after_tip(blade: Blade) -> bool:
	if blade.phase == Blade.YANK and blade.enemy >= 0:
		demo.enemies[blade.enemy].position = blade.tip
		blade.tip += demo.right() * blade.side * BladesDemo.PAIR_GAP
	return false


func next_phase(blade: Blade) -> void:
	match blade.phase:
		Blade.OUT:
			if blade.enemy >= 0:
				demo.hit(blade, blade.enemy, false)
			blade.hold(0.12)
		Blade.HOLD:
			if blade.enemy < 0:
				back(blade)
				return
			var hooked := _hooked_to(blade.enemy)
			for other in hooked:
				if other.phase == Blade.OUT: # the other blade is still on its way: wait for it
					blade.hold(0.02)
					return
			_yank_together(blade, hooked)
		Blade.YANK: # the shared yank ends for both blades at once: land the enemy, reel both home
			var index := blade.enemy
			var enemy := demo.enemies[index]
			enemy.yanked = false
			var arrival := blade.path[3] - blade.path[0] # keep going the way the throw went
			arrival.y = 0.0
			enemy.velocity = arrival.normalized() * float(fling.carry) if arrival.length() > 1e-3 else Vector3.ZERO
			demo.hit(blade, index, false) # lands: hit-stop + flash, no knockback
			for other in demo.blades:
				if other.enemy == index:
					other.enemy = -1
					back(other)
		_:
			super(blade)
