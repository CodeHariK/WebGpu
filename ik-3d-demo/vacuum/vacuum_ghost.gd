## A cute ghost: a glowing white blob with big eyes, pink cheeks, a little "o" mouth and a wispy
## tail — a free-hanging ROPE LimbChain, so it trails behind and swings as the ghost darts about.
## VacuumTug moves it (position, velocity, `pull`); pose() makes it bob, face where it's going,
## shiver and stretch toward the nozzle while it's being sucked (a second-order spring on the
## stretch, so it wobbles when it changes), and shrink away (`shrink`) when it's caught.
class_name VacuumGhost
extends Node3D

const MAX_HEALTH := 100.0
const COLOR := Color(0.95, 0.97, 1.0)
const TAIL_ANCHOR := Vector3(0, -0.12, 0.3) ## where the tail leaves the body (local)

var health := MAX_HEALTH
var velocity := Vector3.ZERO
var pull := Vector3.ZERO ## world direction toward the nozzle while sucked; ZERO = not sucked
var shrink := 1.0 ## 1 = normal size … 0 = gone (sucked in)
var tail: LimbChain

var _body: Node3D
var _time := 0.0
var _heading := SecondOrder.new(2.0, 0.8, 0.0, Vector3.FORWARD)
var _stretch := SecondOrder.new(4.0, 0.3, 1.5) ## x: how much it's stretched toward the nozzle
var _squash := 0.0 ## a kick from a slam, decays


## Build the visuals; the tail is drawn by `renderer`.
func setup(renderer: LimbRenderer) -> void:
	_body = Node3D.new()
	_body.name = "Body"
	add_child(_body)
	_body.add_child(_blob(Vector3(0.42, 0.4, 0.42), Vector3.ZERO, COLOR, 0.35))
	for side: float in [-1.0, 1.0]:
		_body.add_child(_blob(Vector3(0.07, 0.11, 0.04), Vector3(side * 0.14, 0.08, -0.37), Color(0.08, 0.06, 0.12), 0.0))
		_body.add_child(_blob(Vector3(0.025, 0.035, 0.02), Vector3(side * 0.12, 0.13, -0.405), Color.WHITE, 1.0))
		_body.add_child(_blob(Vector3(0.07, 0.04, 0.03), Vector3(side * 0.24, -0.04, -0.33), Color(1.0, 0.55, 0.65), 0.2))
	_body.add_child(_blob(Vector3(0.05, 0.06, 0.03), Vector3(0, -0.1, -0.39), Color(0.25, 0.1, 0.18), 0.0))
	var lengths := PackedFloat32Array()
	lengths.resize(8)
	lengths.fill(0.075)
	tail = LimbChain.new(lengths, to_global(TAIL_ANCHOR), LimbChain.Solver.ROPE)
	tail.pin_tip = false
	tail.rope_gravity = Vector3(0, 0.6, 0) # ghosts float: the tail drifts up a touch
	tail.rope_damping = 0.9
	tail.radius = 0.16
	tail.joint_radius = 0.16
	tail.taper = 0.15
	tail.color = COLOR
	renderer.add(tail)


## Knock it flat for a moment (a slam).
func squash() -> void:
	_squash = 1.0


func pose(delta: float) -> void:
	_time += delta
	_squash = move_toward(_squash, 0.0, delta * 3.0)
	var flat := Vector3(velocity.x, 0.0, velocity.z)
	var want := flat.normalized() if flat.length() > 0.2 else _heading.value
	var heading := _heading.update(delta, want)
	heading.y = 0.0
	heading = heading.normalized() if heading.length() > 0.01 else Vector3.FORWARD
	global_basis = Basis.looking_at(heading)
	var sucked := pull != Vector3.ZERO
	var stretch := _stretch.update(delta, Vector3(0.35 if sucked else 0.0, 0, 0)).x
	if sucked:
		stretch += 0.06 * sin(_time * 38.0) # shivering
	var axis := global_basis.inverse() * pull if sucked else Vector3.FORWARD
	var shape := _stretch_along(axis.normalized(), 1.0 + stretch) * Basis.from_scale(Vector3(1.0 + _squash * 0.5, 1.0 - _squash * 0.5, 1.0 + _squash * 0.5))
	_body.basis = shape.scaled(Vector3.ONE * maxf(shrink, 0.001))
	_body.position = Vector3(0, 0.06 * sin(_time * 2.3), 0)
	visible = shrink > 0.01
	tail.root = _body.global_transform * TAIL_ANCHOR
	tail.radius = 0.16 * shrink
	tail.joint_radius = tail.radius
	tail.time_step = delta
	tail.solve()


# Stretch by `factor` along unit `axis`, thinning across it so the volume stays about the same.
static func _stretch_along(axis: Vector3, factor: float) -> Basis:
	var across := 1.0 / sqrt(maxf(factor, 0.1))
	var k := factor - across
	return Basis(
		Vector3.RIGHT * across + axis * axis.x * k,
		Vector3.UP * across + axis * axis.y * k,
		Vector3.BACK * across + axis * axis.z * k)


static func _blob(size: Vector3, at: Vector3, color: Color, glow: float) -> MeshInstance3D:
	var sphere := SphereMesh.new()
	sphere.radius = 1.0
	sphere.height = 2.0
	sphere.radial_segments = 16
	sphere.rings = 8
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.5
	if glow > 0.0:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = glow
	var instance := MeshInstance3D.new()
	instance.mesh = sphere
	instance.material_override = material
	instance.transform = Transform3D(Basis.from_scale(size), at)
	return instance
