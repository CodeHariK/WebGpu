## The ghost hunter: a round little character with a cap and a vacuum tank on its back, holding
## the nozzle out in front with both hands. All procedural, all LimbChains:
##   arms  two-bone chains, shoulder → elbow → hand, hands gripping the nozzle's handle,
##         elbows hinted down and out
##   hose  a ROPE chain pinned at the tank and at the nozzle's back end: it sags, swings and
##         whips about as the nozzle is yanked around
## The nozzle aims along a second-order spring (it lags and overshoots when the aim swings) and
## rattles while sucking. VacuumTug sets `input`, `aim_target`, `sucking` and drags it about with
## move(); pose() places everything for this frame.
class_name VacuumHunter
extends Node3D

const SPEED := 3.2 ## m/s
const ACCEL := 12.0 ## how fast velocity catches up with the input (1/s)
const SHOULDER := Vector3(0.2, 0.92, -0.02) ## right shoulder (left mirrored), body space
const TANK_TOP := Vector3(0, 1.08, 0.3) ## where the hose leaves the tank
const NOZZLE_AT := Vector3(0, 0.78, -0.42) ## nozzle pivot, body space (aim swings it about this)
const GRIP := Vector3(0.06, -0.03, 0.2) ## right hand on the handle, nozzle space (left mirrored)
const HOSE_SEGMENTS := 14
const HOSE_SLACK := 1.5 ## hose length ÷ straight distance tank → nozzle

var velocity := Vector3.ZERO
var input := Vector3.ZERO ## flat world direction the player is pushing (length ≤ 1)
var aim_target := Vector3.ZERO ## world point to aim the nozzle at; ZERO = straight ahead
var sucking := false
var facing := Vector3.FORWARD ## flat; set by VacuumTug (toward the ghost while capturing)
var nozzle: Node3D ## -Z blows/sucks; its global position is the mouth's pivot
var arms: Array[LimbChain] = []
var hose: LimbChain

var _body: Node3D
var _time := 0.0
var _heading := SecondOrder.new(3.0, 0.75, 0.0, Vector3.FORWARD)
var _aim := SecondOrder.new(3.5, 0.55, 0.3, Vector3(0, -0.15, -1).normalized())
var _walk := 0.0 ## walk cycle phase


func setup(renderer: LimbRenderer) -> void:
	_body = Node3D.new()
	_body.name = "Body"
	add_child(_body)
	_build_body()
	nozzle = _build_nozzle()
	for side: float in [-1.0, 1.0]:
		var arm := LimbChain.new(PackedFloat32Array([0.24, 0.24]))
		arm.radius = 0.05
		arm.joint_radius = 0.055
		arm.color = Color(0.95, 0.55, 0.25)
		renderer.add(arm)
		arms.append(arm)
	var lengths := PackedFloat32Array()
	var straight := (TANK_TOP - NOZZLE_AT).length()
	lengths.resize(HOSE_SEGMENTS)
	lengths.fill(straight * HOSE_SLACK / HOSE_SEGMENTS)
	hose = LimbChain.new(lengths, to_global(TANK_TOP), LimbChain.Solver.ROPE)
	hose.radius = 0.035
	hose.joint_radius = 0.042
	hose.color = Color(0.3, 0.32, 0.36)
	hose.iterations = 20
	hose.rope_damping = 0.93
	renderer.add(hose)


## Walk toward `input` at SPEED (× speed_scale), plus `push` (m/s², e.g. the ghost dragging).
func move(delta: float, push: Vector3, speed_scale: float) -> void:
	var want := input * SPEED * speed_scale
	velocity = velocity.lerp(Vector3(want.x, velocity.y, want.z), 1.0 - exp(-ACCEL * delta))
	velocity += push * delta
	velocity.y = 0.0
	global_position += velocity * delta


func pose(delta: float) -> void:
	_time += delta
	var heading := _heading.update(delta, facing)
	heading.y = 0.0
	heading = heading.normalized() if heading.length() > 0.01 else Vector3.FORWARD
	global_basis = Basis.looking_at(heading)
	var speed := Vector2(velocity.x, velocity.z).length()
	_walk += delta * speed * 4.0
	_body.position = Vector3(0, absf(sin(_walk)) * 0.06 * minf(speed, 1.0), 0)
	_body.rotation.z = sin(_walk) * 0.06 * minf(speed, 1.0)
	_aim_nozzle(delta)
	_pose_arms()
	hose.root = _body.global_transform * TANK_TOP
	hose.target = nozzle.global_transform * Vector3(0, 0, 0.28)
	hose.time_step = delta
	hose.solve()


# The nozzle swings about its pivot toward the aim on a spring, and rattles while sucking.
func _aim_nozzle(delta: float) -> void:
	var pivot := _body.global_transform * NOZZLE_AT
	var want := global_basis * Vector3(0, -0.15, -1).normalized()
	if aim_target != Vector3.ZERO:
		var to_target := aim_target - pivot
		if to_target.length() > 0.2:
			want = to_target.normalized()
	var aim := _aim.update(delta, want).normalized()
	var rattle := Vector3.ZERO
	if sucking:
		rattle = Vector3(sin(_time * 53.0), sin(_time * 61.0 + 1.0), 0.0) * 0.012
	nozzle.global_position = pivot + global_basis * rattle
	if absf(aim.y) < 0.98:
		nozzle.global_basis = Basis.looking_at(aim)


func _pose_arms() -> void:
	var body := _body.global_transform
	for i in 2:
		var side := -1.0 if i == 0 else 1.0
		var arm := arms[i]
		arm.root = body * Vector3(SHOULDER.x * side, SHOULDER.y, SHOULDER.z)
		arm.target = nozzle.global_transform * Vector3(GRIP.x * side, GRIP.y, GRIP.z)
		arm.pole = arm.root + body.basis * Vector3(side * 0.4, -0.5, 0.1) # elbows down and out
		arm.solve()


func _build_body() -> void:
	var overalls := Color(0.25, 0.4, 0.85)
	_body.add_child(_blob(Vector3(0.3, 0.36, 0.27), Vector3(0, 0.62, 0), overalls))
	_body.add_child(_blob(Vector3(0.31, 0.16, 0.28), Vector3(0, 0.86, 0), Color(0.95, 0.55, 0.25))) # shirt
	_body.add_child(_blob(Vector3.ONE * 0.24, Vector3(0, 1.22, -0.02), Color(1.0, 0.82, 0.68))) # head
	_body.add_child(_blob(Vector3(0.25, 0.12, 0.25), Vector3(0, 1.36, 0.0), Color(0.2, 0.62, 0.45))) # cap
	_body.add_child(_blob(Vector3(0.16, 0.03, 0.12), Vector3(0, 1.3, -0.2), Color(0.2, 0.62, 0.45))) # brim
	_body.add_child(_blob(Vector3(0.05, 0.04, 0.04), Vector3(0, 1.18, -0.25), Color(1.0, 0.7, 0.6))) # nose
	for side: float in [-1.0, 1.0]:
		_body.add_child(_blob(Vector3(0.035, 0.05, 0.02), Vector3(side * 0.08, 1.25, -0.22), Color(0.1, 0.08, 0.1)))
		_body.add_child(_blob(Vector3(0.09, 0.07, 0.13), Vector3(side * 0.13, 0.07, -0.03), Color(0.4, 0.25, 0.15))) # shoe
		_body.add_child(_blob(Vector3(0.06, 0.2, 0.06), Vector3(side * 0.13, 0.25, 0.0), overalls)) # leg
	var tank := CylinderMesh.new()
	tank.top_radius = 0.17
	tank.bottom_radius = 0.17
	tank.height = 0.55
	_body.add_child(_mesh(tank, Transform3D(Basis.IDENTITY, Vector3(0, 0.8, 0.33)), Color(0.55, 0.58, 0.62)))
	_body.add_child(_blob(Vector3(0.17, 0.08, 0.17), Vector3(0, 1.08, 0.33), Color(0.55, 0.58, 0.62)))
	_body.add_child(_blob(Vector3.ONE * 0.05, Vector3(0.12, 0.95, 0.47), Color(1.0, 0.85, 0.2), 1.5)) # light


# The nozzle: a short funnel, mouth toward -Z, a handle behind. Top-level, aimed every frame.
func _build_nozzle() -> Node3D:
	var pivot := Node3D.new()
	pivot.name = "Nozzle"
	add_child(pivot)
	pivot.top_level = true
	var funnel := CylinderMesh.new()
	funnel.top_radius = 0.13 # the wide mouth (turned to face -Z below)
	funnel.bottom_radius = 0.06
	funnel.height = 0.32
	pivot.add_child(_mesh(funnel, Transform3D(Basis(Vector3.RIGHT, -PI * 0.5), Vector3(0, 0, -0.1)), Color(0.55, 0.58, 0.62)))
	var handle := CylinderMesh.new()
	handle.top_radius = 0.045
	handle.bottom_radius = 0.045
	handle.height = 0.3
	pivot.add_child(_mesh(handle, Transform3D(Basis(Vector3.RIGHT, PI * 0.5), Vector3(0, 0, 0.15)), Color(0.3, 0.32, 0.36)))
	pivot.add_child(_blob(Vector3(0.13, 0.13, 0.02), Vector3(0, 0, -0.26), Color(0.1, 0.1, 0.12))) # the mouth
	return pivot


static func _blob(size: Vector3, at: Vector3, color: Color, glow := 0.0) -> MeshInstance3D:
	var sphere := SphereMesh.new()
	sphere.radius = 1.0
	sphere.height = 2.0
	sphere.radial_segments = 16
	sphere.rings = 8
	return _mesh(sphere, Transform3D(Basis.from_scale(size), at), color, glow)


static func _mesh(mesh: Mesh, xform: Transform3D, color: Color, glow := 0.0) -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.6
	if glow > 0.0:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = glow
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	instance.transform = xform
	return instance
