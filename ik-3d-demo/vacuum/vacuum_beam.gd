## The suction stream from the nozzle to the ghost. Its shape is a quadratic Bézier curve: it leaves
## the nozzle straight out of the mouth and ends at the ghost, and its middle control point chases
## where it should be on a second-order spring — so when the ghost darts sideways the stream lags,
## bends and wobbles back, cartoon style.
## Drawn by its own translucent glowing LimbRenderer:
##   stream  an FK LimbChain along the curve, thin at the nozzle, wide at the ghost
##   swirl   an FK LimbChain spiralling round the curve; the spiral turns over time, so it reads as
##           air rushing into the nozzle
## update() every frame; `active` false hides it.
class_name VacuumBeam
extends Node3D

const SAMPLES := 12 ## stream segments
const SWIRL_POINTS := 40
const SWIRL_TURNS := 5.0 ## times the spiral winds round between nozzle and ghost
const SWIRL_SPEED := 14.0 ## radians per second the spiral spins

var active := false:
	set(value):
		active = value
		if _renderer != null:
			_renderer.visible = value

var stream: LimbChain
var swirl: LimbChain

var _renderer: LimbRenderer
var _bend := SecondOrder.new(2.5, 0.35, 0.0)
var _time := 0.0
var _was_active := false


func _ready() -> void:
	var glow := StandardMaterial3D.new()
	glow.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	glow.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	glow.vertex_color_use_as_albedo = true
	glow.vertex_color_is_srgb = true
	glow.cull_mode = BaseMaterial3D.CULL_DISABLED
	_renderer = LimbRenderer.new()
	_renderer.name = "Stream"
	_renderer.material = glow
	add_child(_renderer)
	stream = _fk_chain(SAMPLES, 0.05, 4.0, Color(0.6, 0.95, 1.0, 0.22))
	swirl = _fk_chain(SWIRL_POINTS - 1, 0.012, 2.5, Color(0.85, 1.0, 1.0, 0.7))
	_renderer.visible = active


## Bend and draw the stream from `mouth` (the nozzle's tip), leaving along `mouth_dir`, to `to`.
func update(delta: float, mouth: Vector3, mouth_dir: Vector3, to: Vector3) -> void:
	_time += delta
	if not active:
		_was_active = false
		return
	var distance := mouth.distance_to(to)
	var straight := mouth + mouth_dir * distance * 0.5 # where the middle wants to be
	if not _was_active: # switched on: start straight, no swing in from the last position
		_bend.reset(straight)
		_was_active = true
	var middle := _bend.update(delta, straight)
	for i in SAMPLES + 1:
		stream.points[i] = _curve(mouth, middle, to, float(i) / SAMPLES)
	for k in SWIRL_POINTS:
		var t := float(k) / (SWIRL_POINTS - 1)
		var centre := _curve(mouth, middle, to, t)
		var along := (_curve(mouth, middle, to, minf(t + 0.02, 1.0)) - _curve(mouth, middle, to, maxf(t - 0.02, 0.0))).normalized()
		var side := along.cross(Vector3.UP)
		side = side.normalized() if side.length() > 0.01 else Vector3.RIGHT
		var up := side.cross(along)
		var angle := t * TAU * SWIRL_TURNS + _time * SWIRL_SPEED
		swirl.points[k] = centre + (side * cos(angle) + up * sin(angle)) * lerpf(0.04, 0.32, t)
	_renderer.draw()


static func _curve(a: Vector3, control: Vector3, b: Vector3, t: float) -> Vector3:
	var u := 1.0 - t
	return a * (u * u) + control * (2.0 * u * t) + b * (t * t)


func _fk_chain(segments: int, radius: float, taper: float, color: Color) -> LimbChain:
	var lengths := PackedFloat32Array()
	lengths.resize(segments)
	lengths.fill(0.1)
	var chain := LimbChain.new(lengths, Vector3.ZERO, LimbChain.Solver.FK)
	chain.radius = radius
	chain.joint_radius = radius
	chain.taper = taper
	chain.color = color
	_renderer.add(chain)
	return chain
