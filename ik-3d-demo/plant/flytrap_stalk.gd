## One Venus flytrap stalk: a stem of `segments` pieces, posed by angles every frame (forward
## kinematics into an FK LimbChain, drawn by the Flytrap's shared LimbRenderer — no Skeleton3D,
## no BoneAttachment3Ds), topped by a trap head — two jaws on a hinge, teeth round the rims, a
## pink mouth, two little eyes. Flytrap builds and drives it (animate() every frame with the
## shared beat), and sets `renderer` before adding it.
##
##   dance  a sine wave travels up the stem: each bone sways side-to-side (one sway per 2 beats)
##          and front-to-back (half as fast) with a phase lag per bone, growing toward the top —
##          the stem wriggles in a figure-8. The jaws chomp on the beat, the head pulses.
##   look   the head turns part-way toward the target while dancing, fully when hunting
##          (a spring blends the dance pose and the look).
##   snap   target in range → windup (jaws gape, lean back) → snap (lunge at it, jaws slam shut
##          with an overshoot) → chew → back to dancing after a cooldown. The lunge is a
##          second-order spring with r < 0, so it pulls back before it strikes.
class_name FlytrapStalk
extends Node3D

const HEAD_REST_TILT := 0.9 ## radians: at rest the mouth faces up-and-forward, not straight up

# Shape (set by Flytrap before the stalk enters the tree).
var segments := 6
var segment_length := 0.18
var thickness := 0.055 ## stem radius at the base (tapers to 60% at the top)
var head_scale := 1.0
var outward := Vector3.FORWARD ## flat direction the stalk leans toward
var tilt := 0.0 ## radians of lean toward `outward`
var phase := 0.0 ## dance offset, in cycles (side stalks dance out of step)
var stem_color := Color(0.35, 0.62, 0.25)
var jaw_color := Color(0.45, 0.75, 0.3)
var mouth_color := Color(0.85, 0.2, 0.25)
var tooth_color := Color(0.95, 0.95, 0.75)

# Behaviour (Flytrap updates these every frame).
var target: Node3D
var dance := true
var sway := 0.3 ## radians per bone at the top of the stem
var wave := 0.7 ## radians of phase lag per bone: how many wiggles fit on the stem
var snap_range := 2.0 ## metres from the head that triggers a snap
var lunge_angle := 1.1 ## total radians the stem bends toward the target in a snap

var state := "dance"

var _state_time := 0.0
var _cooldown := 0.0
var renderer: LimbRenderer ## draws the stem (set by Flytrap)
var stem: LimbChain ## world-space joint points of the stem, base → tip

var _rest_rotations: Array[Quaternion] = [] ## per segment (only the first carries the lean)
var _tip_basis := Basis.IDENTITY ## world frame of the top segment, where the head sits
var _head_pivot: Node3D ## turns to look; holds the jaws (mouth faces -Z); top_level, placed on the tip
var _upper_jaw: Node3D
var _lower_jaw: Node3D
var _reach := SecondOrder.new(3.0, 0.4, -1.2) ## lunge 0..1: pulls back first (r < 0), overshoots
var _jaw := SecondOrder.new(6.0, 0.35, 2.0) ## jaw opening (radians): slams and bounces
var _look := SecondOrder.new(3.0, 0.7, 0.0) ## how much the head looks at the target (0..1)


func _ready() -> void:
	_build_stem()
	_build_head()
	_pose_stem(0.0, 0.0) # standing up from the first frame, not hanging from the chain's default


## Start a snap now (if not already busy).
func snap() -> void:
	if state == "dance":
		_enter("windup")


## Advance by `delta`; `beat` is the shared dance clock (counts beats).
func animate(delta: float, beat: float) -> void:
	_state_time += delta
	_cooldown -= delta
	var controls := _run_states(beat)
	var reach := _reach.update(delta, Vector3(controls.reach, 0, 0)).x
	_pose_stem(beat, reach)
	_pose_head(delta, beat, controls.look)
	var open := clampf(_jaw.update(delta, Vector3(controls.jaw, 0, 0)).x, -0.1, 1.5)
	_upper_jaw.rotation.x = open * 0.5
	_lower_jaw.rotation.x = -open * 0.5


# What each state wants: jaw opening (radians), lunge (0..1), look weight (0..1).
func _run_states(beat: float) -> Dictionary:
	match state:
		"windup":
			if _state_time >= 0.35:
				_enter("snap")
			return {"jaw": 1.3, "reach": -0.35, "look": 1.0}
		"snap":
			if _state_time >= 0.22:
				_enter("chew")
			return {"jaw": -0.05, "reach": 1.0, "look": 1.0}
		"chew":
			if _state_time >= 0.8:
				_enter("dance")
				_cooldown = 1.6
			return {"jaw": 0.12 + 0.2 * maxf(0.0, sin(TAU * _state_time * 5.0)), "reach": 0.5, "look": 1.0}
	if target != null and _cooldown <= 0.0 and _head_pivot.global_position.distance_to(target.global_position) < snap_range:
		_enter("windup")
	var chomp := 0.35 * maxf(0.0, sin(TAU * beat)) if dance else 0.0 # opens on every beat
	return {"jaw": 0.25 + chomp, "reach": 0.0, "look": 0.55 if target != null else 0.0}


func _enter(new_state: String) -> void:
	state = new_state
	_state_time = 0.0


# The travelling sine wave, plus the lunge: every segment bends a share of lunge_angle toward the
# target, so the whole stem curls at it. Forward kinematics: each segment's frame is its parent's
# times its own rotation, and it reaches segment_length along its +Y.
func _pose_stem(beat: float, reach: float) -> void:
	var bend_axis := Vector3.ZERO
	if target != null:
		var local := global_transform.affine_inverse() * target.global_position
		var flat := Vector3(local.x, 0.0, local.z)
		if flat.length() > 0.01:
			bend_axis = Vector3.UP.cross(flat.normalized()) # rotating +Y about this tips it toward the target
	var frame := global_basis.orthonormalized()
	stem.points[0] = global_position
	for i in segments:
		var k := float(i + 1) / segments # stronger toward the top
		var rotation_now := Quaternion.IDENTITY
		if dance:
			var side := sin(TAU * (beat * 0.5 + phase) - i * wave) * sway * k
			var nod := sin(TAU * (beat * 0.25 + phase) - i * wave + 1.3) * sway * 0.6 * k
			rotation_now = Quaternion.from_euler(Vector3(nod, 0.0, side))
		if bend_axis != Vector3.ZERO:
			rotation_now = Quaternion(bend_axis, reach * lunge_angle / segments) * rotation_now
		frame = frame * Basis(_rest_rotations[i] * rotation_now)
		stem.points[i + 1] = stem.points[i] + frame * Vector3(0, segment_length, 0)
	_tip_basis = frame


# Blend the head between its dance pose (riding the stem tip) and looking straight at the target.
func _pose_head(delta: float, beat: float, look_weight: float) -> void:
	var weight := clampf(_look.update(delta, Vector3(look_weight, 0, 0)).x, 0.0, 1.0)
	_head_pivot.global_position = stem.tip()
	var rest := _tip_basis.orthonormalized() * Basis(Vector3.RIGHT, HEAD_REST_TILT)
	var aim := rest
	if target != null:
		var to_target := target.global_position - _head_pivot.global_position
		if to_target.length() > 0.05:
			aim = Basis.looking_at(to_target.normalized(), Vector3.UP)
	var pulse := 1.0 + (0.06 * maxf(0.0, sin(TAU * beat)) if dance else 0.0)
	_head_pivot.global_basis = rest.slerp(aim, weight).scaled(Vector3.ONE * head_scale * pulse)


# --- build -----------------------------------------------------------------------------------

# The stem chain (segment 0 carries the lean) and its look; posed by _pose_stem() every frame.
func _build_stem() -> void:
	var lengths := PackedFloat32Array()
	lengths.resize(segments)
	lengths.fill(segment_length)
	stem = LimbChain.new(lengths, global_position, LimbChain.Solver.FK)
	stem.radius = thickness
	stem.joint_radius = thickness # cylinders + same-size balls = a capsule stem
	stem.taper = 0.6
	stem.color = stem_color
	renderer.add(stem)
	var lean_axis := Vector3.UP.cross(outward.normalized())
	for i in segments:
		var lean := Basis.IDENTITY
		if i == 0 and tilt != 0.0 and lean_axis.length() > 0.01:
			lean = Basis(lean_axis.normalized(), tilt)
		_rest_rotations.append(lean.get_rotation_quaternion())


func _build_head() -> void:
	_head_pivot = Node3D.new()
	_head_pivot.name = "Head"
	add_child(_head_pivot)
	_head_pivot.top_level = true
	_upper_jaw = _build_jaw(1.0)
	_lower_jaw = _build_jaw(-1.0)
	for side: float in [-1.0, 1.0]: # eyes on top of the upper jaw
		var eye := _mesh(_sphere(0.055), Transform3D(Basis.IDENTITY, Vector3(0.09 * side, 0.1, -0.02)), Color(0.97, 0.97, 0.92))
		_upper_jaw.add_child(eye)
		eye.add_child(_mesh(_sphere(0.028), Transform3D(Basis.IDENTITY, Vector3(0, 0.015, -0.042)), Color(0.05, 0.05, 0.08)))


# A jaw: hinge at the back of the head (+Z), a flattened green shell reaching forward (-Z), a pink
# mouth inside, and teeth round the rim pointing at the other jaw (staggered so they interlock).
# `side` = +1 upper, -1 lower.
func _build_jaw(side: float) -> Node3D:
	var hinge := Node3D.new()
	hinge.name = "UpperJaw" if side > 0.0 else "LowerJaw"
	hinge.position = Vector3(0, 0, 0.12)
	_head_pivot.add_child(hinge)
	var centre := Vector3(0, 0.035 * side, -0.15)
	hinge.add_child(_mesh(_sphere(1.0), Transform3D(Basis.from_scale(Vector3(0.3, 0.1, 0.28)), centre), jaw_color))
	hinge.add_child(_mesh(_sphere(1.0), Transform3D(Basis.from_scale(Vector3(0.26, 0.05, 0.24)), Vector3(0, 0.012 * side, -0.15)), mouth_color))
	var count := 9
	for k in count:
		var t := (k + (0.25 if side > 0.0 else 0.75)) / float(count) # lower teeth sit between upper ones
		var angle := lerpf(-PI * 0.55, PI * 0.55, t)
		var rim := Vector3(sin(angle) * 0.29, 0.015 * side, -0.15 - cos(angle) * 0.27)
		var direction := (Vector3(sin(angle), 0.0, -cos(angle)) * 0.35 + Vector3(0, -side, 0)).normalized()
		var cone := CylinderMesh.new()
		cone.top_radius = 0.0
		cone.bottom_radius = 0.018
		cone.height = 0.1
		hinge.add_child(_mesh(cone, Transform3D(_basis_along(direction), rim + direction * 0.05), tooth_color))
	return hinge


static func _sphere(radius: float) -> SphereMesh:
	var sphere := SphereMesh.new()
	sphere.radius = radius
	sphere.height = radius * 2.0
	return sphere


static func _basis_along(y: Vector3) -> Basis:
	var helper := Vector3.FORWARD if absf(y.dot(Vector3.FORWARD)) < 0.9 else Vector3.RIGHT
	var x := y.cross(helper).normalized()
	return Basis(x, y, x.cross(y))


static func _mesh(mesh: Mesh, xform: Transform3D, color: Color) -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.7
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	instance.transform = xform
	return instance
