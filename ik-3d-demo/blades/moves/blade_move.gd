## Base of every blade move. A move starts itself (start() on its key), then the demo plays each
## blade's path and asks the move through these hooks:
##   live_path(blade)      a path that must follow the player / enemy while it plays (empty: keep)
##   after_tip(blade)      after the tip moved; true = stop this blade's update for the frame
##   hits_during(phase)    whether the blade hits enemies in this phase
##   chain_on_path(blade)  OUT: does the chain lie on the travelled path (else it is taut)
##   knocks_back()         hits push the enemy away
##   hitstop_scale()       how long a hit freezes the game (× BladesDemo.HITSTOP)
##   after_hit(blade)      right after one of this move's hits
##   next_phase(blade)     the phase's time is up: what comes next
## plus per-frame tick(delta), draw_debug(debug) and reset() for moves with their own state.
##
## It also holds what every attack shares: the wind-up + strike launch, the reel back to the hand,
## and the paired throw of both blades.
class_name BladeMove
extends RefCounted

var demo: BladesDemo
var key := "" ## its name in BladesDemo.moves (set by each move)


func _init(owner: BladesDemo) -> void:
	demo = owner


# --- hooks (override as needed) -------------------------------------------------------------------

func reset() -> void:
	pass


func tick(_delta: float) -> void:
	pass


func draw_debug(_debug: BladeDebug) -> void:
	pass


func live_path(_blade: Blade) -> PackedVector3Array:
	return PackedVector3Array()


func after_tip(_blade: Blade) -> bool:
	return false


func hits_during(phase: int) -> bool:
	return phase == Blade.OUT


func chain_on_path(_blade: Blade) -> bool:
	return true


func knocks_back() -> bool:
	return true


func hitstop_scale() -> float:
	return 1.0


func after_hit(_blade: Blade) -> void:
	pass


## Default order: out → a short hold in the target → back → idle.
func next_phase(blade: Blade) -> void:
	match blade.phase:
		Blade.OUT:
			blade.hold(0.06)
		Blade.BACK:
			blade.phase = Blade.IDLE
		_:
			back(blade)


# --- shared by every attack ---------------------------------------------------------------------

## The blade now plays this move; the right blade leaves a little later (they're never in sync).
func begin(
		blade: Blade,
		enemy := -1
) -> void:
	blade.move = key
	blade.enemy = enemy
	blade.hit.clear()
	blade.delay = BladesDemo.STAGGER if blade.side > 0.0 else 0.0


## Both blades whip out wide on their own sides and come down on the aim, side by side.
func throw_both(
		aim: Vector3,
		enemy := -1
) -> void:
	for blade in demo.blades:
		begin(blade, enemy)
		var hand := demo.hand(blade.side)
		var target := aim + demo.right() * blade.side * BladesDemo.PAIR_GAP
		var distance := hand.distance_to(target)
		# Comes down onto the target from above and out on its own side (its tilted plane).
		var p2 := target + swing_up(blade) * distance * 0.25
		launch(blade, PackedVector3Array([hand, hand, p2, target]), 0.15 + distance * 0.008, "out")


## Each blade swings in its own plane: it holds your forward direction and an "up" that leans out
## to the blade's side by the swing tilt — so the left blade's arcs bulge up-left and never cross
## over to the right, and the other way round.
func swing_up(blade: Blade) -> Vector3:
	var tilt := deg_to_rad(demo.swing_tilt)
	return (Vector3.UP * cos(tilt) + demo.right() * blade.side * sin(tilt)).normalized()


## Every attack's OUT path = wind-up segment + strike segment, one smooth curve, in the blade's plane:
##   wind-up  hand → top (up the tilted plane, top_behind back); its last handle sits behind the
##            top, so the blade arrives at the top moving forward
##   strike   top → target; its first handle sits ahead of the top on the same line (the tangents
##            match: no kink), its last two points come from the attack's own `path` (p2, p3)
## The wind-up eases into the top (slows: the hang), the strike uses the attack's easing.
## Control points 0–3 = wind-up, 4–7 = strike (3 and 4 are the same point: the join).
func launch(
		blade: Blade,
		path: PackedVector3Array,
		duration: float,
		easing: String
) -> void:
	var hand := demo.hand(blade.side)
	var up := swing_up(blade)
	var top := wind_top(blade)
	var forward := demo.facing
	var reach := top.distance_to(path[3])
	var windup := [hand, hand + up * 0.6 - forward * demo.top_behind * 0.5, top - forward * 0.8, top]
	var strike := [top, top + forward * reach * 0.35, path[2], path[3]]
	blade.start_segments(Blade.OUT, PackedVector3Array(windup + strike),
			PackedFloat32Array([demo.windup_time, duration]), ["smooth", easing])


## The top of the wind-up: up the blade's tilted plane to top_height, top_behind back.
func wind_top(blade: Blade) -> Vector3:
	var hand := demo.hand(blade.side)
	var up := swing_up(blade)
	return hand + up * (demo.top_height - BladesDemo.HAND_HEIGHT) / maxf(up.y, 0.2) - demo.facing * demo.top_behind


## Start the same path on several blades on one clock. `caller` is the blade whose update is
## running now (it already stepped this frame); blades updated after it this frame would step
## once more, so they start one step back — and all of them stay on exactly the same point.
func start_together(
		blades: Array[Blade],
		caller: Blade,
		phase: int,
		path: PackedVector3Array,
		duration: float,
		easing: String
) -> void:
	var caller_index := demo.blades.find(caller)
	for blade in blades:
		blade.start(phase, path, duration, easing)
		if demo.blades.find(blade) > caller_index:
			blade.time = -demo.game_delta


## Reel the blade home along a curve that bows out on its own side.
func back(blade: Blade) -> void:
	var hand := demo.hand(blade.side)
	var right := demo.right() * blade.side
	var distance := hand.distance_to(blade.tip)
	var p1 := blade.tip + Vector3.UP * distance * 0.2 + right * distance * 0.25
	var p2 := hand + demo.facing * distance * 0.3 + right * distance * 0.35
	blade.start(Blade.BACK, PackedVector3Array([blade.tip, p1, p2, hand]), 0.12 + distance * 0.01, "smooth")
