## Ground anchor (J): in the air, both blades are hurled straight down into the ground under you
## (a little ahead, the way you're drifting), then the chains haul you down onto them, faster and
## faster — and the landing smashes everything within shock: knocked away and up, harder the closer
## it was. On the ground, J first leaps you up (toward the mouse) and anchors at the top of the leap.
##
## Like RushMove, the player rides a Bezier while hauled (BladesDemo stops moving you meanwhile);
## the blades sit stuck in the ground (HOLD) until the landing, then reel home.
class_name AnchorMove
extends BladeMove

var leap := 11.5 ## m/s up for the leap from the ground (≈2.6 m high) …
var leap_forward := 3.0 ## … and toward the mouse
const MIN_HEIGHT := 0.4 ## m: higher than this counts as in the air (anchor at once)
var ahead := 0.3 ## s: the blades land where your drift would take you in this long
var spread := 0.6 ## m: each blade lands this far out to its own side
var down_time := 0.14 ## s: blades from your hands into the ground
var shock := 3.5 ## m: the landing knocks away every enemy this close …
var knock := 11.0 ## … this hard at the centre (m/s, falling off to 4 at the edge)

var hauling := false ## the chains are pulling you down (you ride _path)
var _leaping := false ## leapt from the ground: anchor at the top of the leap
var _spot := Vector3.ZERO ## the landing (on the ground)
var _path := PackedVector3Array() ## your feet while hauled: 4 control points
var _time := 0.0
var _duration := 0.0


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "anchor"


## The tuning panel's sliders for this move: [var name, min, max, step].
func tunables() -> Array:
	return [
		["leap", 4, 20, 0.5],
		["leap_forward", 0, 10, 0.5],
		["ahead", 0, 1, 0.05],
		["spread", 0, 2, 0.05],
		["down_time", 0.05, 0.5, 0.01],
		["shock", 1, 8, 0.1],
		["knock", 2, 25, 0.5],
	]


func reset() -> void:
	hauling = false
	_leaping = false


## J: anchor now (in the air) or leap first (on the ground).
func start() -> void:
	if hauling or _leaping or not demo.blades_free():
		return
	if demo.player.y > MIN_HEIGHT:
		_anchor()
		return
	var toward := demo.mouse_point() - demo.player
	toward.y = 0.0
	demo.player_velocity = Vector3.UP * leap + toward.normalized() * leap_forward if toward.length() > 0.3 else Vector3.UP * leap
	demo.player.y = 0.001 # in the air, so BladesDemo flies you
	_leaping = true


## Both blades are stuck in the ground: haul.
func next_phase(blade: Blade) -> void:
	if blade.phase != Blade.OUT:
		super(blade)
		return
	blade.hold(99.0)
	for other in demo.blades:
		if other.move == key and other.phase != Blade.HOLD:
			return
	_haul()


func tick(delta: float) -> void:
	if _leaping and demo.player_velocity.y <= 0.5: # the top of the leap
		_leaping = false
		if demo.blades_free():
			_anchor()
	if not hauling:
		return
	_time += delta
	var t := clampf(_time / _duration, 0.0, 1.0)
	demo.player = Bezier.point(_path[0], _path[1], _path[2], _path[3], Blade.eased_time(t, "in"))
	if t >= 1.0:
		_land()


func hits_during(_phase: int) -> bool:
	return false


func draw_debug(debug: BladeDebug) -> void:
	if not (hauling or _leaping or _spot != Vector3.ZERO):
		return
	if hauling:
		debug.curve_lines(_path, Color(0.4, 1.0, 0.6))
	var last := _spot + Vector3(shock, 0.05, 0.0)
	for i in range(1, 33): # the shock radius on the ground
		var a := TAU * i / 32.0
		var p := _spot + Vector3(cos(a) * shock, 0.05, sin(a) * shock)
		debug.line(last, p, Color(1.0, 0.5, 0.2))
		last = p


# Hurl both blades down into the ground under you (where your drift is taking you), either side.
func _anchor() -> void:
	var drift := demo.player_velocity
	drift.y = 0.0
	_spot = demo.player + drift * ahead
	_spot.y = 0.0
	for blade in demo.blades:
		begin(blade)
		blade.delay = 0.0 # both at once: it's a reaction
		var hit := _spot + demo.right() * blade.side * spread
		var from := blade.tip
		var path := PackedVector3Array([from, from + Vector3.DOWN * 0.4 + demo.right() * blade.side * 0.3, hit + Vector3.UP * 1.0, hit])
		blade.start_even(Blade.OUT, path, down_time, "in")


# The chains pull you down onto the spot: slow for a beat, then violent.
func _haul() -> void:
	var from := demo.player
	var height := maxf(from.y, 0.5)
	_path = PackedVector3Array([from, from + Vector3.UP * 0.3, _spot + Vector3.UP * height * 0.4, _spot])
	_duration = 0.14 + height * 0.025
	_time = 0.0
	hauling = true
	demo.player_velocity = Vector3.ZERO


# Impact: everything within shock is blown away and up; big freeze and shake; blades reel home.
func _land() -> void:
	hauling = false
	demo.player = _spot
	demo.player_velocity = Vector3.ZERO
	demo.shake = 0.6
	demo.hitstop = BladesDemo.HITSTOP * 2.5
	for i in 8: # a ring of sparks
		var a := TAU * i / 8.0
		demo.fire.spark(_spot + Vector3(cos(a), 0.2, sin(a)) * 1.2)
	for enemy in demo.enemies:
		if enemy.yanked or enemy.bound > 0.0:
			continue
		var away := enemy.position - _spot
		away.y = 0.0
		var distance := away.length()
		if distance > shock:
			continue
		var push := lerpf(knock, 4.0, distance / shock)
		enemy.velocity = (away / distance if distance > 1e-3 else Vector3.RIGHT) * push + Vector3.UP * (push * 0.6)
		enemy.flash = 1.0
		enemy.squash = maxf(enemy.squash, 0.5)
	for blade in demo.blades:
		if blade.move == key and blade.phase != Blade.IDLE:
			back(blade)
	_spot = Vector3.ZERO
