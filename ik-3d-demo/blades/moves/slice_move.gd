## Slice (F): a combo of slashes on the enemy nearest the mouse, the blades taking turns. Each blade
## winds up, slashes, then reels its chain fully home before its next slash; the right blade runs
## half a cycle behind, so one slashes while the other reels in.
## Each slash lies in its own random plane through the line hand → enemy, leaning toward the
## blade's own side. Slashes don't knock back (the enemy stays for the combo) and freeze shorter.
class_name SliceMove
extends BladeMove

const SLICES := 6 ## slashes per combo (3 per blade)
const TIME := 0.17
const SIZE := 1.6 ## how far from the enemy's line a slash starts
const RANGE := 9.0
const TILT_MIN := 15.0 ## a slash plane leans this many degrees (or more) toward its blade's side …
const TILT_MAX := 65.0 ## … and at most this many

var enemy := -1


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "slice"


func start() -> void:
	if not demo.blades_free():
		return
	var target := demo.enemy_near_mouse(RANGE, false)
	if target < 0:
		return
	enemy = target
	for blade in demo.blades:
		begin(blade)
		blade.slice_angles.clear()
	for k in SLICES: # alternate blades; each slash in its own random plane
		demo.blades[(k + 1) % 2].slice_angles.append(deg_to_rad(randf_range(TILT_MIN, TILT_MAX)))
	demo.blades[1].delay = (demo.windup_time + 0.08 + TIME + 0.2) * 0.5
	for blade in demo.blades:
		slash_out(blade)


## One slash from the hand: wind up, then out (over the shoulder) to the slash's start.
func slash_out(blade: Blade) -> void:
	var start_point := slice_path(blade)[0]
	var hand := demo.hand(blade.side)
	var lift := Vector3.UP * 0.8
	launch(blade, PackedVector3Array([hand, hand.lerp(start_point, 0.33) + lift, hand.lerp(start_point, 0.66) + lift, start_point]), 0.08, "accelerate")


## One slash: down through the enemy from above and out on the blade's own side, in a plane that
## holds the line hand → enemy (x) and leans toward the blade's side by its random angle.
##   along x: starts and ends short of the enemy (near), handles past it (far), so at the middle
##            (Bezier at t = ½: (p0 + 3·p1 + 3·p2 + p3) / 8) it is exactly at the enemy's distance
##   across:  starts high-outside (o0), ends straight below (o3); the handles o1, o2 are chosen so
##            o0 + 3·o1 + 3·o2 + o3 = 0, i.e. the middle is exactly ON the enemy. A symmetric loop
##            would end as far over the other side as it started; this one only dips a little past.
func slice_path(blade: Blade) -> PackedVector3Array:
	var target := demo.enemies[enemy].position
	var hand := demo.hand(blade.side)
	var x := target - hand
	var distance := x.length()
	x = x / distance if distance > 1e-3 else demo.facing
	var up := (Vector3.UP - x * x.dot(Vector3.UP)).normalized()
	var side := demo.right() * blade.side
	side = (side - x * x.dot(side)).normalized()
	var angle := blade.slice_angles[0]
	var o0 := (up * cos(angle) + side * sin(angle)) * SIZE # high, out on its own side
	var o3 := -up * SIZE * 0.8 # straight below
	var o1 := o0 * 0.5
	var o2 := -(o0 + o1 * 3.0 + o3) / 3.0
	var near := hand + x * distance * 0.55
	var far := hand + x * distance * (1.0 - 0.55 / 4.0) / 0.75
	return PackedVector3Array([near + o0, far + o1, far + o2, near + o3])


func live_path(blade: Blade) -> PackedVector3Array:
	return slice_path(blade) if blade.phase == Blade.SLICE else PackedVector3Array()


func hits_during(phase: int) -> bool:
	return phase == Blade.OUT or phase == Blade.SLICE


func knocks_back() -> bool:
	return false


func hitstop_scale() -> float:
	return 0.5


## Only the reel after the last slash ends the combo.
func done_after_back(blade: Blade) -> bool:
	return blade.slice_angles.is_empty()


func next_phase(blade: Blade) -> void:
	match blade.phase:
		Blade.OUT:
			blade.hit.clear()
			blade.start(Blade.SLICE, slice_path(blade), TIME, "smooth")
		Blade.SLICE: # reel the chain fully home before the next slash
			blade.slice_angles.pop_front()
			back(blade)
		Blade.BACK:
			if blade.slice_angles.is_empty():
				blade.phase = Blade.IDLE
			else:
				slash_out(blade)
		_:
			super(blade)
