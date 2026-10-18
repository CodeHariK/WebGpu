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
##   done_after_back(blade) BACK: is this the last reel of the move (then a new move may cut in)
## plus per-frame tick(delta), draw_debug(debug) and reset() for moves with their own state, and
## tunables(): the move's numbers the tuning panel (Tab) edits and MoveTuning saves.
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


## The tuning panel's sliders for this move: [var name, min, max, step] per tunable var.
func tunables() -> Array:
	return []


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


## Whether this reel home ends the move: then the blade counts as free (BladesDemo.blades_free) and
## the next move can start from wherever it is on the way back. False for a reel between the
## steps of a combo (a slice reels home between slashes).
func done_after_back(_blade: Blade) -> bool:
	return true


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
		var p2 := target + swing_up(blade) * distance * demo.drop
		launch(blade, PackedVector3Array([hand, hand, p2, target]), 0.15 + distance * 0.008, "swing")


## Each blade swings in its own plane: it holds your forward direction and an "up" that leans out
## to the blade's side by the swing tilt — so the left blade's arcs bulge up-left and never cross
## over to the right, and the other way round.
func swing_up(blade: Blade) -> Vector3:
	var tilt := deg_to_rad(demo.swing_tilt)
	return (Vector3.UP * cos(tilt) + demo.right() * blade.side * sin(tilt)).normalized()


## Every attack's OUT path: ONE curve from the hand, back through the wind point behind you and on
## to the attack's target, played with ONE easing along its length (Blade.start_even) — so there is
## no stop or speed jump between wind-up and strike.
##   hand → wind point   leaves the hand going back and up, and comes into the wind point already
##                       heading for the target (its handle sits behind the point on that line)
##   wind point → target the handle on the far side continues that same line (the tangents match:
##                       no kink); the last two points come from the attack's own `path` (p2, p3)
## The handles either side of the wind point are the same share (demo.strike_handle) of their piece's length,
## so the speed along the curve is the same on both sides of it too. Control points 0–3 and 4–7
## (3 and 4 are the wind point). `easing` runs over the whole curve ("swing": slow back, fast
## through, soft landing; "accelerate": still speeding up at the end, into a spin or sweep).
## Time: windup_time + the attack's `duration`.
func launch(
		blade: Blade,
		path: PackedVector3Array,
		duration: float,
		easing: String
) -> void:
	var hand := blade.tip # where it hangs right now (BladesDemo.rest): the curve starts there, no jump
	var wind := wind_point(blade)
	var target := path[3]
	var through := (target - hand).normalized() # the direction the blade passes the wind point in
	var leave := (swing_up(blade) - demo.facing).normalized() # back and up, in the blade's own plane
	var first := hand.distance_to(wind) * demo.windup_bow # the hand → wind piece bows out: longer than straight
	var second := wind.distance_to(target)
	var handle := demo.strike_handle
	var windup := [hand, hand + leave * first * handle, wind - through * first * handle, wind]
	var strike := [wind, wind + through * second * handle, path[2], target]
	blade.start_even(Blade.OUT, PackedVector3Array(windup + strike), demo.windup_time + duration, easing)
	blade.swing_end = 0.5 # the wind point: the end of the first segment


## The wind point: behind you (against your facing) by top_behind, out to the blade's own side by
## wind_side, top_height above the ground — so the left blade winds up behind-left, the right one
## behind-right.
func wind_point(blade: Blade) -> Vector3:
	return demo.player + Vector3.UP * demo.top_height - demo.facing * demo.top_behind + demo.right() * blade.side * demo.wind_side


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
	var hand := demo.rest(blade.side) # (kept on it every frame while reeling: BladesDemo._update_blade)
	var right := demo.right() * blade.side
	var distance := hand.distance_to(blade.tip)
	var p1 := blade.tip + Vector3.UP * distance * 0.2 + right * distance * 0.25
	var p2 := hand + demo.facing * distance * 0.3 + right * distance * 0.35
	blade.start(Blade.BACK, PackedVector3Array([blade.tip, p1, p2, hand]), 0.12 + distance * 0.01, "smooth")
