## Rip (K): both blades bite into the top of the heavy slab nearest the mouse; then every K is a tug —
## you brace and heave, the slab jolts toward you and creaks back a little less each time — and on the
## tugs_needed-th it tears out of the ground, flies over and lands on its side in front of you, smashing
## anyone near the landing. Stop tugging for give_up seconds and you let go (it settles back).
##
## The slab's tilt is a SecondOrder spring (snappy, overshoots): each tug raises its goal and kicks
## it, so it lurches past and settles. The blades hold (HOLD) on the slab's grip points, moved with
## it every frame (they ride it through the flight too), and reel home when it lands.
class_name RipMove
extends BladeMove

const PROPS := [Vector3(-6, 0, -3), Vector3(6, 0, -5), Vector3(-2, 0, -12)] ## where the slabs stand
const RANGE := 14.0
var tugs_needed := 3
var tug_tilt := 0.18 ## radians: how far each tug leaves it leaning toward you …
var tug_kick := 2.5 ## … and how hard it lurches (radians / s) on the tug
var give_up := 2.5 ## seconds without a tug: let go
var land_ahead := 2.0 ## m: it lands this far in front of you (on its way past)
var flight := 0.75 ## seconds in the air (≈1.5 m high arc)
var splash := 2.2 ## m: enemies this close to the landing are knocked away

var prop := -1 ## the slab being worked on (demo.props)
var hooked := false ## both blades are in it
var tugs := 0
var tug_time := 0.0 ## seconds left of the last tug's heave (for the body's lean)

var _idle := 0.0
var _goal := 0.0
var _tilt := SecondOrder.new(4.0, 0.3, 1.0)


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "rip"


## The tuning panel's sliders for this move: [var name, min, max, step].
func tunables() -> Array:
	return [
		["tugs_needed", 1, 8, 1],
		["tug_tilt", 0, 0.6, 0.02],
		["tug_kick", 0, 8, 0.1],
		["give_up", 0.5, 6, 0.1],
		["land_ahead", 0.5, 5, 0.1],
		["flight", 0.3, 1.5, 0.05],
		["splash", 0, 5, 0.1],
	]


func reset() -> void:
	prop = -1
	hooked = false
	tugs = 0


## K: hook the slab nearest the mouse, or (hooked) tug.
func start() -> void:
	if hooked and prop >= 0 and demo.props[prop].stuck:
		_tug()
		return
	if prop >= 0 or not demo.blades_free():
		return
	var index := _prop_near_mouse()
	if index < 0:
		return
	prop = index
	tugs = 0
	var slab: RipProp = demo.props[index]
	var to_you := demo.player - slab.base
	to_you.y = 0.0
	slab.axis = Vector3.UP.cross(to_you.normalized()) # +tilt tips its top toward you; also your right
	_goal = 0.0
	_tilt.reset(Vector3(slab.tilt, 0.0, 0.0))
	throw_both(slab.point(RipProp.SIZE.y * 0.85, 0.0))


## Each blade bites in and holds; when both are in, you're hooked.
func next_phase(blade: Blade) -> void:
	if blade.phase != Blade.OUT:
		super(blade)
		return
	blade.hold(99.0)
	for other in demo.blades:
		if other.move == key and other.phase != Blade.HOLD:
			return
	hooked = true
	_idle = 0.0


func tick(delta: float) -> void:
	tug_time = maxf(tug_time - delta, 0.0)
	if prop < 0 or delta <= 0.0:
		return
	var slab: RipProp = demo.props[prop]
	if slab.flying:
		slab.tilt = move_toward(slab.tilt, PI / 2.0, delta * PI / 2.0 / flight) # turning onto its side
		if slab.update(delta):
			_landed(slab)
			return
	elif slab.stuck:
		slab.tilt = _tilt.update(delta, Vector3(_goal, 0.0, 0.0)).x
		if hooked:
			_idle += delta
			if _idle > give_up:
				_let_go()
		elif _goal == 0.0 and absf(slab.tilt) < 0.01 and not _blades_in():
			slab.tilt = 0.0
			prop = -1 # settled back after letting go
			return
	for blade in demo.blades: # the bites move with the slab
		if blade.move == key and blade.phase == Blade.HOLD:
			var at := slab.grip(blade.side)
			blade.path = PackedVector3Array([at, at, at, at])
			blade.tip = at


func hits_during(_phase: int) -> bool:
	return false


func draw_debug(debug: BladeDebug) -> void:
	if prop >= 0:
		var slab: RipProp = demo.props[prop]
		debug.label(slab.point(RipProp.SIZE.y + 0.4, 0.0), "tug %d/%d" % [tugs, tugs_needed], Color(1.0, 0.8, 0.4))


# One heave: the slab lurches toward you and settles leaning further; the last one rips it out.
func _tug() -> void:
	tugs += 1
	_idle = 0.0
	tug_time = 0.25
	var slab: RipProp = demo.props[prop]
	demo.shake = 0.2 + 0.05 * tugs
	demo.hitstop = BladesDemo.HITSTOP * 0.6
	demo.fire.spark(slab.base + Vector3.UP * 0.2)
	if tugs < tugs_needed:
		_goal = tug_tilt * tugs
		_tilt.kick(Vector3(tug_kick, 0.0, 0.0))
		return
	_rip(slab)


# Torn out: fly over to just in front of you, turning onto its side.
func _rip(slab: RipProp) -> void:
	slab.stuck = false
	slab.flying = true
	var toward := slab.base - demo.player
	toward.y = 0.0
	var land := demo.player + toward.normalized() * land_ahead
	land.y = 0.0
	slab.base.y = 0.01
	slab.velocity = (land - slab.base) / flight + Vector3.UP * (0.5 * RipProp.GRAVITY * flight)
	demo.shake = 0.4
	for i in 4:
		demo.fire.spark(slab.base + Vector3(randf_range(-0.6, 0.6), 0.1, randf_range(-0.6, 0.6)))


# It lands: thud, knock away anyone near, the blades reel home.
func _landed(slab: RipProp) -> void:
	slab.tilt = PI / 2.0
	demo.shake = 0.5
	demo.hitstop = BladesDemo.HITSTOP * 2.0
	for enemy in demo.enemies:
		if enemy.yanked or enemy.bound > 0.0:
			continue
		var away := enemy.position - slab.base
		away.y = 0.0
		if away.length() < splash:
			enemy.velocity = away.normalized() * 8.0 + Vector3.UP * 4.0
			enemy.flash = 1.0
	_reel_home()
	hooked = false
	prop = -1
	tugs = 0


# Gave up: reel home; the slab springs back upright.
func _let_go() -> void:
	hooked = false
	_goal = 0.0
	tugs = 0
	_reel_home()


func _reel_home() -> void:
	for blade in demo.blades:
		if blade.move == key and blade.phase != Blade.IDLE:
			back(blade)


func _blades_in() -> bool:
	for blade in demo.blades:
		if blade.move == key and (blade.phase == Blade.OUT or blade.phase == Blade.HOLD):
			return true
	return false


# The stuck slab nearest the mouse point (within BladesDemo.PICK_RADIUS) and RANGE of you; −1: none.
func _prop_near_mouse() -> int:
	var aim := demo.mouse_point()
	var best := -1
	var best_distance := BladesDemo.PICK_RADIUS
	for i in demo.props.size():
		var slab: RipProp = demo.props[i]
		if not slab.stuck or slab.base.distance_to(demo.player) > RANGE:
			continue
		var d := Vector2(slab.base.x - aim.x, slab.base.z - aim.z).length()
		if d < best_distance:
			best = i
			best_distance = d
	return best
