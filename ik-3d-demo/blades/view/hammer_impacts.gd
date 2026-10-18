## The hammers' impacts in the blades demo (live, not replayable), built on HammerFX:
##   impact()   a crash: scrap / rubble chunks and sparks thrown out (HammerFX bursts), a scorch on
##              the ground for the big ones, and for a heavy one the camera lurching in (fov_kick)
##              and a moment of slow motion (slow_left: BladesDemo plays the game slower meanwhile)
##   trails     a smear behind each hammer head while it moves fast: a ribbon through where the
##              ball was over the last TRAIL_FRAMES frames, as wide as the ball
class_name HammerImpacts
extends Node3D

const SLOW_TIME := 0.18 ## seconds of slow motion after a heavy impact …
const SLOW_SPEED := 0.3 ## … at this speed
const PUNCH := 9.0 ## degrees the camera's view narrows on a heavy impact (then springs back)
const TRAIL_FRAMES := 10
const TRAIL_SPEED := 8.0 ## m/s: slower than this, no smear
const LIFE := 3.0 ## seconds a burst is kept

var slow_left := 0.0 ## seconds of slow motion still to come

var _fx := HammerFX.new()
var _bursts: Array = []
var _crater_at := Vector3.ZERO
var _crater_radius := 0.0
var _crater_since := -1.0
var _punch := -1.0 ## seconds since the last heavy impact (−1: none)
var _history := {} ## blade → Array of [ball centre, half its width across, the step] per frame, newest first
var _trail := ImmediateMesh.new()


func _ready() -> void:
	add_child(_fx)
	var trail := MeshInstance3D.new()
	trail.mesh = _trail
	var smear := StandardMaterial3D.new()
	smear.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	smear.vertex_color_use_as_albedo = true
	smear.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	smear.cull_mode = BaseMaterial3D.CULL_DISABLED
	trail.material_override = smear
	add_child(trail)


## A crash at `at`: `chunks` pieces of `color` and `sparks` sparks flung out at up to `power` m/s; a
## scorch `crater` m wide (0: none); `heavy`: the camera lurches and time slows.
func impact(
		at: Vector3,
		chunks: int,
		sparks: int,
		power: float,
		color: Color,
		crater := 0.0,
		heavy := true
) -> void:
	var burst := HammerFX.Burst.new()
	burst.origin = at
	burst.seed = randi()
	burst.chunks = chunks
	burst.sparks = sparks
	burst.power = power
	burst.chunk_color = color
	_bursts.append(burst)
	if crater > 0.0:
		_crater_at = at
		_crater_radius = crater
		_crater_since = 0.0
	if heavy:
		_punch = 0.0
		slow_left = SLOW_TIME


## How many degrees to narrow the camera's view now.
func fov_kick() -> float:
	return PUNCH * exp(-_punch * 9.0) if _punch >= 0.0 else 0.0


## A frame: the bursts and the scorch age by the game's `delta` (they freeze in a hit-stop), the
## camera's lurch by the real `view_delta`; the heads' smears follow `blades` (when `hammers`).
func update(
		delta: float,
		view_delta: float,
		blades: Array[Blade],
		hammers: bool
) -> void:
	for burst: HammerFX.Burst in _bursts:
		burst.since += delta
	_bursts = _bursts.filter(func(b: HammerFX.Burst) -> bool: return b.since < LIFE)
	_fx.draw_bursts(_bursts)
	if _crater_since >= 0.0:
		_crater_since += delta
		if _crater_since > 4.0:
			_crater_since = -1.0
	_fx.draw_crater(_crater_at, _crater_radius, _crater_since)
	if _punch >= 0.0:
		_punch += view_delta
		if _punch > 1.0:
			_punch = -1.0
	_draw_trails(delta, blades, hammers)


func clear() -> void:
	_bursts.clear()
	_crater_since = -1.0
	_history.clear()
	slow_left = 0.0


func _draw_trails(
		delta: float,
		blades: Array[Blade],
		hammers: bool
) -> void:
	_trail.clear_surfaces()
	if not hammers:
		_history.clear()
		return
	var begun := false
	for blade in blades:
		var frames: Array = _history.get(blade, [])
		if delta > 0.0:
			var ball := blade.head.global_transform
			frames.push_front([ball.origin + ball.basis.y * -HammerHead.RADIUS, ball.basis.x * HammerHead.RADIUS, delta])
			if frames.size() > TRAIL_FRAMES:
				frames.resize(TRAIL_FRAMES)
			_history[blade] = frames
		for k in frames.size() - 1:
			var now: Array = frames[k]
			var then: Array = frames[k + 1]
			var speed: float = (now[0] as Vector3).distance_to(then[0]) / maxf(now[2], 1e-4)
			var fade := clampf((speed - TRAIL_SPEED) / TRAIL_SPEED, 0.0, 1.0) * (1.0 - float(k) / frames.size())
			if fade <= 0.01:
				continue
			if not begun:
				_trail.surface_begin(Mesh.PRIMITIVE_TRIANGLES)
				begun = true
			var near := Color(0.85, 0.78, 0.7, 0.35 * fade)
			var far := Color(0.85, 0.78, 0.7, 0.35 * fade * (1.0 - 1.0 / frames.size()))
			var a: Vector3 = now[0] - now[1]
			var b: Vector3 = now[0] + now[1]
			var c: Vector3 = then[0] + then[1]
			var d: Vector3 = then[0] - then[1]
			for p: Array in [[a, near], [b, near], [c, far], [a, near], [c, far], [d, far]]:
				_trail.surface_set_color(p[1])
				_trail.surface_add_vertex(p[0])
	if begun:
		_trail.surface_end()
