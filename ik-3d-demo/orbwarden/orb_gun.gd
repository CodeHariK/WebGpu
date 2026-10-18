## The Orb Warden's gun turret, sitting on top of the sphere: a squat base, a head that yaws and
## pitches toward the target (rate-limited, so you can outrun its aim), twin barrels and a glowing
## sight. burst() fires BURST bolts, alternating barrels, each kicking its barrel back. Bolts are
## glowing slugs that fly in a straight line; one passing close to the target emits
## `bolt_hit(direction)`, one hitting the ground throws sparks.
## `glows` are the materials OrbParts lights to show the turret's health.
class_name OrbGun
extends Node3D

signal bolt_hit(direction: Vector3)

const BURST := 5
const GAP := 0.1 ## seconds between bolts
const SPEED := 26.0 ## m/s
const LIFE := 2.0 ## seconds a bolt flies
const SPREAD := 0.035 ## radians of random aim error
const TURN := 2.2 ## rad/s the head turns at most
const HIT_RADIUS := 0.55
const KICK := 0.18 ## barrel recoil (m)
const SIGHT := Color(1.0, 0.5, 0.1)

var target: Node3D
var sparks: OrbSparks
var ground_mask := 1
var enabled := true ## false = broken: no aiming, no firing
var glows: Array[StandardMaterial3D] = []

var _yaw: Node3D
var _pitch: Node3D
var _barrels: Array[Node3D] = []
var _recoil := [0.0, 0.0]
var _left := 0 ## shots left in the burst
var _wait := 0.0
var _next_barrel := 0
var _bolts: Array = [] ## [MeshInstance3D, velocity, age]


func _ready() -> void:
	_build()


func busy() -> bool:
	return _left > 0


func burst() -> void:
	if enabled and target != null and _left == 0:
		_left = BURST
		_wait = 0.25 # a beat to line up


func stop() -> void:
	_left = 0


func _process(delta: float) -> void:
	_move_bolts(delta)
	for i in 2:
		_recoil[i] = move_toward(_recoil[i], 0.0, delta * 1.2)
		_barrels[i].position.z = -0.25 + _recoil[i]
	if not enabled or target == null:
		_left = 0
		return
	_aim(delta)
	if _left > 0:
		_wait -= delta
		if _wait <= 0.0:
			_fire()
			_left -= 1
			_wait = GAP


# Turn the head toward the target, no faster than TURN rad/s on each axis.
func _aim(delta: float) -> void:
	var local := _yaw.get_parent_node_3d().global_transform.affine_inverse() * target.global_position
	var yaw := atan2(-local.x, -local.z)
	_yaw.rotation.y = _yaw.rotation.y + clampf(wrapf(yaw - _yaw.rotation.y, -PI, PI), -TURN * delta, TURN * delta)
	var head := _pitch.global_transform.affine_inverse() * target.global_position
	var pitch := atan2(head.y, -head.z)
	_pitch.rotation.x = clampf(_pitch.rotation.x + clampf(pitch, -TURN * delta, TURN * delta), -1.2, 0.5)


func _fire() -> void:
	var barrel := _barrels[_next_barrel]
	_recoil[_next_barrel] = KICK
	_next_barrel = 1 - _next_barrel
	var muzzle := barrel.global_transform * Vector3(0, 0, -0.35)
	var direction := -barrel.global_basis.z.normalized()
	direction = direction.rotated(Vector3.UP, randf_range(-SPREAD, SPREAD))
	direction = direction.rotated(direction.cross(Vector3.UP).normalized(), randf_range(-SPREAD, SPREAD))
	var mesh := CapsuleMesh.new()
	mesh.radius = 0.06
	mesh.height = 0.6
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = Color(2.5, 1.2, 0.4)
	var bolt := MeshInstance3D.new()
	bolt.mesh = mesh
	bolt.material_override = material
	bolt.top_level = true
	bolt.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(bolt)
	bolt.global_transform = Transform3D(_along(direction), muzzle)
	_bolts.append([bolt, direction * SPEED, 0.0])
	if sparks != null:
		sparks.burst(muzzle, direction, 5, 6.0, 0.4, 0.15, 0.03, global_position.y - 10.0, Color(1.0, 0.85, 0.5))


func _move_bolts(delta: float) -> void:
	var space := get_world_3d().direct_space_state
	for i in range(_bolts.size() - 1, -1, -1):
		var bolt: MeshInstance3D = _bolts[i][0]
		var velocity: Vector3 = _bolts[i][1]
		_bolts[i][2] += delta
		var from := bolt.global_position
		var to := from + velocity * delta
		var done: bool = _bolts[i][2] > LIFE
		if target != null and _segment_distance(target.global_position, from, to) < HIT_RADIUS:
			bolt_hit.emit(velocity.normalized())
			done = true
		else:
			var ground := space.intersect_ray(PhysicsRayQueryParameters3D.create(from, to, ground_mask))
			if not ground.is_empty():
				if sparks != null:
					sparks.burst(ground.position, ground.normal, 6, 4.0, 0.7, 0.3, 0.03, ground.position.y)
				done = true
		if done:
			bolt.queue_free()
			_bolts.remove_at(i)
		else:
			bolt.global_position = to


static func _segment_distance(point: Vector3, a: Vector3, b: Vector3) -> float:
	return point.distance_to(Geometry3D.get_closest_point_to_segment(point, a, b))


static func _along(direction: Vector3) -> Basis:
	var y := direction.normalized()
	var x := y.cross(Vector3.UP if absf(y.y) < 0.99 else Vector3.RIGHT).normalized()
	return Basis(x, y, x.cross(y))


func _build() -> void:
	var dark := OrbEye._material(Color(0.1, 0.1, 0.11), 0.4, 0.8)
	var metal := OrbEye._material(Color(0.22, 0.23, 0.25), 0.35, 0.85)
	var base := CylinderMesh.new()
	base.top_radius = 0.35
	base.bottom_radius = 0.45
	base.height = 0.25
	add_child(OrbEye._mesh(base, Transform3D.IDENTITY, dark))
	_yaw = Node3D.new()
	_yaw.position.y = 0.2
	add_child(_yaw)
	_pitch = Node3D.new()
	_pitch.position.y = 0.12
	_yaw.add_child(_pitch)
	var head := BoxMesh.new()
	head.size = Vector3(0.5, 0.3, 0.45)
	_pitch.add_child(OrbEye._mesh(head, Transform3D.IDENTITY, metal))
	var sight_material := OrbEye._material(SIGHT, 0.2, 0.0)
	sight_material.emission_enabled = true
	sight_material.emission = SIGHT
	sight_material.emission_energy_multiplier = 3.0
	glows.append(sight_material)
	var sight := BoxMesh.new()
	sight.size = Vector3(0.16, 0.08, 0.04)
	_pitch.add_child(OrbEye._mesh(sight, Transform3D(Basis.IDENTITY, Vector3(0, 0.07, -0.23)), sight_material))
	for side: float in [-1.0, 1.0]:
		var barrel := Node3D.new()
		barrel.position = Vector3(0.14 * side, -0.04, -0.25)
		_pitch.add_child(barrel)
		var tube := CylinderMesh.new()
		tube.top_radius = 0.045
		tube.bottom_radius = 0.055
		tube.height = 0.6
		barrel.add_child(OrbEye._mesh(tube, Transform3D(Basis(Vector3.RIGHT, PI * 0.5), Vector3(0, 0, -0.1)), dark))
		_barrels.append(barrel)
