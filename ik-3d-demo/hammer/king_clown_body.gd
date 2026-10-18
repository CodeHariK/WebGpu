## KingClown as a stand-in until the real model: a rigid body, one mesh per bone (no skinning),
## posed by code every frame — the way the game's IK characters will be built.
##
## Proportions: short legs, a big round belly, wide shoulders, long thick arms with big gloves, a
## big head with a red nose, orange hair puffs, a ruff collar and a gold crown. Every size is a
## slider on the tuning panel's "Body" tab (metres), applied live — the IK takes the new lengths
## at once.
##
## Each frame, from the attack's pose:
##   pelvis    above his feet; a squash lowers it (the knees bend: the legs are IK), a stretch
##             raises it a little; the belly squashes and stretches too (the cartoon part)
##   spine     tips forward / back with the lean, and the chest turns with the twist
##   arms      two-bone IK (LimbChain) from each shoulder to its grip on the handle; elbows out
##             and down
##   legs      two-bone IK from each hip: on the ground the feet stand (or walk: a simple stride
##             cycle while moving); in the air they tuck up under him, more the higher he is
##   head      looks at what he's after, turning at most MAX_LOOK from his chest
##   hammer    HammerBody (the same hammer and grips as every body)
class_name KingClownBody
extends HammerBody

const ANKLE := 0.1 ## ankle above the sole
const STRIDE := 0.32 ## m each foot swings forward / back while walking
const STEP_LENGTH := 0.9 ## m walked per full stride cycle
const MAX_LOOK := 1.1 ## radians the head turns away from the chest at most
const SUIT := Color(0.55, 0.2, 0.75) ## purple
const TRIM := Color(1.0, 0.8, 0.15) ## yellow
const SKIN := Color(1.0, 0.86, 0.78)

var pelvis := 0.74 ## hip joints above the feet, standing (the legs a little bent)
var spine := 0.8 ## hips → shoulder line
var neck := 0.4 ## shoulder line → head centre
var shoulder_width := 0.4 ## half the shoulder width
var hip_width := 0.18 ## half the hip width
var upper_arm := 0.6 ## long gorilla arms: they reach round the belly
var forearm := 0.56
var arm_thickness := 0.12 ## the upper arm's radius (the forearm a bit less)
var glove := 0.17 ## the gloves' radius
var thigh := 0.4
var shin := 0.38
var leg_thickness := 0.11
var belly := 0.5 ## the belly's radius
var head_size := 0.3 ## the head's radius

var _arms: Array[LimbChain] = []
var _legs: Array[LimbChain] = []
var _belly: Node3D
var _collar: Node3D
var _head: Node3D
var _bones := {} ## "upper_arm0" … → the rigid part (its origin at the bone's start, +Y along it)
var _sizes := {} ## "upper_arm0" … → [length, radius] the part's mesh was last built at
var _stride := 0.0 ## the walk cycle's phase (radians)
var _last_feet := Vector3.ZERO


func _ready() -> void:
	for i in 2:
		_arms.append(LimbChain.new(PackedFloat32Array([upper_arm, forearm]), Vector3.ZERO, LimbChain.Solver.TWO_BONE))
		_legs.append(LimbChain.new(PackedFloat32Array([thigh, shin]), Vector3.ZERO, LimbChain.Solver.TWO_BONE))
		_bones["upper_arm%d" % i] = _limb(SUIT)
		_bones["forearm%d" % i] = _limb(TRIM)
		_bones["glove%d" % i] = _ball(1.0, Color.WHITE) # unit: scaled to `glove`
		_bones["thigh%d" % i] = _limb(SUIT)
		_bones["shin%d" % i] = _limb(TRIM)
		_bones["shoe%d" % i] = _shoe()
	_belly = _build_belly()
	_collar = _build_collar()
	_head = _build_head()
	_build_hammer()


func _init() -> void:
	key = "king_clown"


func shoulder_height() -> float:
	return pelvis + spine


func live_shoulders() -> bool:
	return true


func tunables() -> Array:
	return [
		"Body",
		["pelvis", 0.4, 1.2, 0.01], ["spine", 0.4, 1.2, 0.01], ["neck", 0.2, 0.7, 0.01],
		["shoulder_width", 0.2, 0.7, 0.01], ["hip_width", 0.08, 0.35, 0.01],
		["upper_arm", 0.25, 0.9, 0.01], ["forearm", 0.25, 0.9, 0.01], ["arm_thickness", 0.04, 0.2, 0.005], ["glove", 0.06, 0.3, 0.005],
		["thigh", 0.2, 0.7, 0.01], ["shin", 0.2, 0.7, 0.01], ["leg_thickness", 0.04, 0.2, 0.005],
		["belly", 0.25, 0.8, 0.01], ["head_size", 0.15, 0.5, 0.01],
	]


func apply(
		pose: SmashAttack.Pose,
		attack: SmashAttack,
		look_at_point: Vector3,
		moving: bool
) -> void:
	var forward := pose.forward
	var right := SmashAttack.right_of(forward)
	_hold_hammer(pose, attack)
	_size_parts()
	# pelvis and spine
	var air := clampf((pose.feet.y - SmashAttack.GROUND) / 0.35, 0.0, 1.0) # 0 standing … 1 well off the ground
	var hips := pose.feet + Vector3.UP * (pelvis + clampf(pose.squash - 1.0, -0.5, 0.2) * 1.4)
	var up := Vector3.UP.rotated(right, -pose.lean) # the spine: a forward lean tips it toward his front
	var chest_right := right.rotated(up, pose.twist)
	var chest_forward := up.cross(chest_right)
	var chest := hips + up * spine * clampf(pose.squash, 0.8, 1.1)
	_place(_belly, hips + up * spine * 0.45, Basis(chest_right, up, -chest_forward), _belly_scale(pose.squash) * belly / 0.5)
	_place(_collar, chest + up * 0.05, Basis(chest_right, up, -chest_forward), Vector3.ONE)
	_place_head(chest + up * neck, chest_forward, up, look_at_point)
	# arms: from each shoulder to its grip
	for i in 2:
		var side := -1.0 if i == 0 else 1.0
		var shoulder := chest + chest_right * side * shoulder_width - up * 0.05
		var arm := _arms[i]
		arm.root = shoulder
		arm.target = hand_targets[i]
		arm.pole = shoulder + Vector3.DOWN * 0.4 + chest_right * side * 0.5 - chest_forward * 0.15
		arm.solve()
		_segment(_bones["upper_arm%d" % i], arm.points[0], arm.points[1], chest_right)
		_segment(_bones["forearm%d" % i], arm.points[1], arm.points[2], chest_right)
		_place(_bones["glove%d" % i], arm.points[2], Basis.IDENTITY, Vector3.ONE * glove)
	# legs: standing / walking on the ground, tucked up in the air
	_walk_cycle(pose.feet, moving)
	for i in 2:
		var side := -1.0 if i == 0 else 1.0
		var hip := hips + right * side * hip_width
		var phase := _stride + (0.0 if i == 0 else PI)
		var swing := sin(phase) * STRIDE if moving else 0.0
		var lift := maxf(cos(phase), 0.0) * 0.14 if moving else 0.0
		var stand := pose.feet + right * side * (hip_width + 0.04) + forward * swing + Vector3.UP * (ANKLE + lift)
		var legs := thigh + shin
		var tuck := hip + Vector3.DOWN * legs * (0.58 if pose.rising else 0.7) + forward * legs * (0.36 if pose.rising else 0.15)
		var leg := _legs[i]
		leg.root = hip
		leg.target = stand.lerp(tuck, air)
		leg.pole = hip + forward * 0.8 + Vector3.DOWN * 0.2 # knees forward
		leg.solve()
		_segment(_bones["thigh%d" % i], leg.points[0], leg.points[1], right)
		_segment(_bones["shin%d" % i], leg.points[1], leg.points[2], right)
		_place(_bones["shoe%d" % i], leg.points[2], Basis(right, Vector3.UP, -forward), Vector3.ONE)


func arm_points(i: int) -> PackedVector3Array:
	return _arms[i].points


# The sliders' sizes onto the IK chains and the parts' meshes (rebuilt only when they changed).
func _size_parts() -> void:
	for i in 2:
		_arms[i].lengths = PackedFloat32Array([upper_arm, forearm])
		_legs[i].lengths = PackedFloat32Array([thigh, shin])
		_size_limb("upper_arm%d" % i, upper_arm, arm_thickness)
		_size_limb("forearm%d" % i, forearm, arm_thickness * 0.85)
		_size_limb("thigh%d" % i, thigh, leg_thickness)
		_size_limb("shin%d" % i, shin, leg_thickness * 0.78)


func _size_limb(
		name: String,
		length: float,
		radius: float
) -> void:
	var size := [length, radius]
	if _sizes.get(name) == size:
		return
	_sizes[name] = size
	var instance: MeshInstance3D = _bones[name].get_child(0)
	var mesh: CapsuleMesh = instance.mesh
	mesh.radius = radius
	mesh.height = length + radius * 2.0 # its round ends reach the joints
	instance.position = Vector3.UP * length * 0.5


# The walk cycle: its phase moves with the distance walked (so the feet don't slide).
func _walk_cycle(
		feet: Vector3,
		moving: bool
) -> void:
	var walked := Vector2(feet.x - _last_feet.x, feet.z - _last_feet.z).length()
	_last_feet = feet
	if moving and walked < 1.0: # (not a jump to a new spot)
		_stride = fmod(_stride + walked / STEP_LENGTH * TAU, TAU)
	elif not moving:
		_stride = 0.0


# The head at `at`, facing what he looks at — but never more than MAX_LOOK from his chest.
func _place_head(
		at: Vector3,
		chest_forward: Vector3,
		up: Vector3,
		look_at_point: Vector3
) -> void:
	var look := (look_at_point - at).normalized() if look_at_point.distance_to(at) > 0.1 else chest_forward
	var angle := chest_forward.angle_to(look)
	if angle > MAX_LOOK:
		look = chest_forward.slerp(look, MAX_LOOK / angle).normalized()
	var x := up.cross(look).normalized() # his left… the face is +Z
	if x.length() < 1e-3:
		x = Vector3.RIGHT
	var y := look.cross(x).normalized()
	_place(_head, at, Basis(x, y, look), Vector3.ONE * head_size / 0.3)


# Squash: shorter and wider (the volume about the same).
static func _belly_scale(squash: float) -> Vector3:
	var s := clampf(squash, 0.6, 1.4)
	var w := 1.0 / sqrt(s)
	return Vector3(w, s, w)


# A rigid part along a bone from a to b: origin at a, +Y toward b, +X along `across` (made square).
func _segment(
		part: Node3D,
		a: Vector3,
		b: Vector3,
		across: Vector3
) -> void:
	var y := (b - a).normalized()
	var x := (across - y * across.dot(y))
	x = x.normalized() if x.length() > 1e-3 else y.cross(Vector3.FORWARD).normalized()
	_place(part, a, Basis(x, y, x.cross(y)), Vector3.ONE)


func _place(
		part: Node3D,
		at: Vector3,
		basis: Basis,
		scale_by: Vector3
) -> void:
	part.global_transform = Transform3D(basis.orthonormalized() * Basis.from_scale(scale_by), at)


# --- the parts (each a top-level Node3D holding its meshes) ---------------------------------------

func _part() -> Node3D:
	var part := Node3D.new()
	part.top_level = true
	add_child(part)
	return part


# A limb bone: a capsule from the origin up +Y (sized by _size_limb).
func _limb(color: Color) -> Node3D:
	var part := _part()
	_add(part, CapsuleMesh.new(), color, Vector3.ZERO)
	return part


func _ball(
		radius: float,
		color: Color
) -> Node3D:
	var part := _part()
	_add(part, _sphere(radius), color, Vector3.ZERO)
	return part


# A big red clown shoe, its heel under the ankle, its toe well out in front (+Z… −Z is forward).
func _shoe() -> Node3D:
	var part := _part()
	var mesh := CapsuleMesh.new()
	mesh.radius = 0.11
	mesh.height = 0.5
	var shoe := _add(part, mesh, Color(0.9, 0.12, 0.15), Vector3(0, -ANKLE + 0.09, -0.12))
	shoe.rotation = Vector3(PI * 0.5, 0, 0)
	shoe.scale = Vector3(1.0, 1.0, 0.75) # flatter
	return part


func _build_belly() -> Node3D:
	var part := _part()
	var belly := _add(part, _sphere(0.5), SUIT, Vector3.ZERO)
	belly.scale = Vector3(1.0, 1.05, 0.9)
	for k in 3: # yellow pom-pom buttons down his front
		_add(part, _sphere(0.07), TRIM, Vector3(0, 0.25 - k * 0.22, -0.47 + absf(k - 1) * 0.04))
	return part


# The ruff: a wide white ring round his neck.
func _build_collar() -> Node3D:
	var part := _part()
	var ruff := TorusMesh.new()
	ruff.inner_radius = 0.1
	ruff.outer_radius = 0.34
	var ring := _add(part, ruff, Color.WHITE, Vector3.ZERO)
	ring.scale = Vector3(1.0, 0.6, 1.0)
	return part


# The head (its face along +Z): skin ball, red nose, eyes, a smile, orange hair puffs, a crown.
func _build_head() -> Node3D:
	var part := _part()
	_add(part, _sphere(0.3), SKIN, Vector3.ZERO)
	_add(part, _sphere(0.085), Color(0.95, 0.1, 0.1), Vector3(0, -0.02, 0.3))
	for x in [-0.11, 0.11]:
		_add(part, _sphere(0.065), Color.WHITE, Vector3(x, 0.09, 0.25))
		_add(part, _sphere(0.032), Color(0.05, 0.05, 0.1), Vector3(x, 0.09, 0.31))
		var puff := _add(part, _sphere(0.16), Color(1.0, 0.45, 0.05), Vector3(x * 2.6, 0.06, -0.04))
		puff.scale = Vector3(0.8, 1.0, 1.0)
	var smile := _add(part, _sphere(0.11), Color(0.85, 0.1, 0.15), Vector3(0, -0.15, 0.22))
	smile.scale = Vector3(1.3, 0.35, 0.5)
	var crown := CylinderMesh.new()
	crown.top_radius = 0.17
	crown.bottom_radius = 0.15
	crown.height = 0.12
	_add(part, crown, Color(1.0, 0.78, 0.1), Vector3(0, 0.3, 0))
	for k in 5: # the crown's points
		var a := k * TAU / 5.0
		var spike := PrismMesh.new()
		spike.size = Vector3(0.09, 0.12, 0.04)
		var point := _add(part, spike, Color(1.0, 0.78, 0.1), Vector3(sin(a) * 0.15, 0.42, cos(a) * 0.15))
		point.rotation = Vector3(0, a, 0)
	return part


func _add(
		part: Node3D,
		mesh: Mesh,
		color: Color,
		at: Vector3
) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	instance.material_override = material
	instance.position = at
	part.add_child(instance)
	return instance


static func _sphere(radius: float) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	return mesh
