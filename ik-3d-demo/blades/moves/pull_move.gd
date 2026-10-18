## Pull (E): both blades hook the enemy nearest the mouse, hit-stop, then YANK it along a lifted
## arc (ease-in: slow, then violent) to a random spot: your feet, flung off to your left or right,
## or over your head to land behind you. The first blade in waits for the second; then both yank
## together on one curve, side by side, and let go together when it lands.
class_name PullMove
extends BladeMove

const RANGE := 12.0

var fling := {} ## where the yanked enemy goes {name, to, lift, duration, carry}, picked once per pull

# The four flings (tuning panel): at your feet / to your side / over your head behind you.
var which := 0 ## 0: a random one each pull, 1 feet, 2 left, 3 right, 4 over your head
var feet_distance := 1.6 ## m in front of you
var feet_lift := 2.0
var feet_time := 0.32
var side_distance := 4.5 ## m out to the side (and 0.5 forward)
var side_lift := 2.5
var side_time := 0.36
var side_carry := 5.0 ## m/s it keeps sliding after landing
var over_distance := 5.0 ## m behind you
var over_lift := 5.0
var over_time := 0.48
var over_carry := 4.0


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "pull"


## The tuning panel's sliders for this move: [var name, min, max, step].
func tunables() -> Array:
	return [
		["which", 0, 4, 1],
		["feet_distance", 0.5, 4, 0.1], ["feet_lift", 0, 6, 0.1], ["feet_time", 0.1, 1, 0.02],
		["side_distance", 1, 8, 0.1], ["side_lift", 0, 6, 0.1], ["side_time", 0.1, 1, 0.02], ["side_carry", 0, 12, 0.5],
		["over_distance", 1, 10, 0.1], ["over_lift", 0, 10, 0.1], ["over_time", 0.1, 1.2, 0.02], ["over_carry", 0, 12, 0.5],
	]


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
		{"name": "at your feet", "to": forward * feet_distance, "lift": feet_lift, "duration": feet_time, "carry": 0.0},
		{"name": "flung left", "to": -right * side_distance + forward * 0.5, "lift": side_lift, "duration": side_time, "carry": side_carry},
		{"name": "flung right", "to": right * side_distance + forward * 0.5, "lift": side_lift, "duration": side_time, "carry": side_carry},
		{"name": "over your head", "to": -forward * over_distance, "lift": over_lift, "duration": over_time, "carry": over_carry},
	]
	return choices[which - 1] if which > 0 else choices[randi() % choices.size()]


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


## The yank's curve from `from` to `to`: it starts off along the chain (toward your hands — the taut
## chain flows straight into the arc, no kink), lifts, comes over and down onto the spot.
func _yank_path(
		from: Vector3,
		to: Vector3,
		lift: float
) -> PackedVector3Array:
	var hands := demo.player + Vector3.UP * BladesDemo.HAND_HEIGHT
	var along := (hands - from).normalized()
	var p1 := from + along * lift * 0.7 + Vector3.UP * lift * 0.55
	return PackedVector3Array([from, p1, to + Vector3.UP * lift * 0.8, to])


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
