## Builds a spider entirely in code from a SpiderLayout: a Skeleton3D (body + 3 bones per leg),
## capsule visuals attached to the bones, eyes, and one TwoBoneIK3D driving every leg.
##
##   bones:  body ─┬─ <leg>_upper ── <leg>_lower ── <leg>_tip     (per leg in the layout)
##   IK:     per leg, root = upper (hip joint), middle = lower (knee), end = tip (foot)
##           target = a foot Marker3D in world space (moved by SpiderLeg)
##           pole   = a Marker3D above-and-outside the knee, so knees always bend up/out
##
## No animation and no Blender model: the IK bends the legs from the rest pose every frame.
## A Blender spider would replace only the bones + visuals; the gait code stays the same.
class_name SpiderRig
extends RefCounted

const POLE_OFFSET := Vector3(0, 1.0, 0) ## pole sits this far above the knee, plus outward

var layout: SpiderLayout
var skeleton: Skeleton3D
var ik: TwoBoneIK3D
var targets: Array[Marker3D] = [] ## foot targets, world space (top_level)
var poles: Array[Marker3D] = [] ## knee-direction hints, ride along with the body
var knee_bones: Array[int] = [] ## the `<leg>_lower` bone of each leg (for debug drawing)
var eyes: Array[Node3D] = [] ## eye pivots: -Z looks out, scale.y blinks
var eye_materials: Array[StandardMaterial3D] = [] ## eyeball materials (colour / glow)
var pupils: Array[MeshInstance3D] = []
var head: Node3D ## a detached head (layout.head), top_level: SpiderSprings moves it; null = none
var coil: Array[MeshInstance3D] = [] ## rings of the spring coil between neck and head
var rigid_limbs: Array[Node3D] = [] ## per leg: its one-piece visual if the leg is rigid, else null
## Per leg: world point a rigid leg hangs from instead of its hip on the body (SpiderSprings'
## hip_spring sets it); null or missing = the hip on the body.
var rigid_hips: Array = []
var gun_barrels: Array[Node3D] = [] ## head gun (layout.head.gun): one pivot per barrel, head space, -Z = firing direction
var muzzles: Array[Node3D] = [] ## the tip of each barrel, where shots and flashes come from


## `body` is the node that carries the spider's height/tilt; the skeleton goes under it.
static func build(body: Node3D, spider_layout: SpiderLayout) -> SpiderRig:
	var rig := SpiderRig.new()
	rig.layout = spider_layout
	rig.skeleton = Skeleton3D.new()
	rig.skeleton.name = "Skeleton3D"
	body.add_child(rig.skeleton)
	var body_bone := rig.skeleton.add_bone("body")
	rig._add_body_visual(body_bone)
	for leg in spider_layout.legs:
		rig._add_leg(leg, body_bone, body)
	rig._add_ik()
	return rig


## Aim every rigid (one-bone) leg at its foot: the leg's rest shape (hip → foot, both blocks)
## turns about the hip by the shortest rotation that points it at the foot target, and
## stretches along that line to reach it — by at most ±max_stretch, so it never goes rubbery;
## past that the foot falls a little short. Call after the feet have moved (Spider does).
func update_rigid_limbs() -> void:
	for leg in rigid_limbs.size():
		var limb := rigid_limbs[leg]
		if limb == null:
			continue
		var definition := layout.legs[leg]
		var body := skeleton.global_transform
		var hip: Vector3 = body * definition.hip
		if leg < rigid_hips.size() and rigid_hips[leg] != null:
			hip = rigid_hips[leg]
		var rest := body.basis * (definition.rest_foot() - definition.hip) # rest hip → foot, world
		var now := targets[leg].global_position - hip
		if now.length() < 0.01 or rest.length() < 0.01:
			continue
		var turn := Basis(Quaternion(rest.normalized(), now.normalized()))
		var stretch := clampf(now.length() / rest.length(), 1.0 - definition.max_stretch, 1.0 + definition.max_stretch)
		limb.global_transform = Transform3D(turn * body.basis * _stretch_along(definition.rest_foot() - definition.hip, stretch), hip)


## Turn the knee poles on/off (off = the IK picks the bend direction on its own).
func set_poles_enabled(enabled: bool) -> void:
	for leg in layout.legs.size():
		ik.set_pole_node(leg, ik.get_path_to(poles[leg]) if enabled else NodePath())


# Three bones per leg. Rest rotations stay identity: the IK reads each bone's direction from
# where its child sits, so only the positions matter.
func _add_leg(leg: SpiderLayout.LegDef, body_bone: int, body: Node3D) -> void:
	var upper := _add_bone(leg.name + "_upper", body_bone, leg.hip)
	var lower := _add_bone(leg.name + "_lower", upper, leg.upper_vec())
	_add_bone(leg.name + "_tip", lower, leg.lower_vec())
	knee_bones.append(lower)
	if leg.rigid: # one piece aimed by update_rigid_limbs(); the bones stay (IK-driven) but unseen
		rigid_limbs.append(_add_rigid_limb(leg, body))
	else:
		rigid_limbs.append(null)
		if leg.square: # blocky, and floating apart at the joints when segment_gap > 0 (detached limbs)
			_add_block_visual(upper, leg.upper_vec(), leg.radius, leg.segment_gap)
			_add_block_visual(lower, leg.lower_vec(), leg.radius, leg.segment_gap)
		else:
			_add_segment_visual(upper, leg.upper_vec(), leg.radius)
			_add_segment_visual(lower, leg.lower_vec(), leg.radius)
	if not leg.walks:
		_add_pincer(lower, leg)

	var target := Marker3D.new()
	target.name = leg.name + "_foot_target"
	target.top_level = true # world space: stays planted while the body moves
	body.add_child(target)
	target.global_position = body.global_transform * leg.rest_foot()
	targets.append(target)

	var pole := Marker3D.new()
	pole.name = leg.name + "_knee_pole"
	if leg.bend != Vector3.ZERO: # a mammal limb: knee/elbow points where the layout says
		pole.position = leg.hip + leg.upper_vec() + leg.bend * 1.0
	else: # a spider leg: knee up and out
		pole.position = leg.hip + leg.upper_vec() + leg.out * 0.3 + POLE_OFFSET
	body.add_child(pole)
	poles.append(pole)


func _add_bone(bone_name: String, parent: int, offset: Vector3) -> int:
	var bone := skeleton.add_bone(bone_name)
	skeleton.set_bone_parent(bone, parent)
	skeleton.set_bone_rest(bone, Transform3D(Basis.IDENTITY, offset))
	skeleton.set_bone_pose(bone, skeleton.get_bone_rest(bone))
	return bone


func _add_ik() -> void:
	ik = TwoBoneIK3D.new()
	ik.name = "LegIK"
	skeleton.add_child(ik)
	ik.set_setting_count(layout.legs.size())
	for leg in layout.legs.size():
		var leg_name := layout.legs[leg].name
		ik.set_root_bone_name(leg, leg_name + "_upper")
		ik.set_middle_bone_name(leg, leg_name + "_lower")
		ik.set_end_bone_name(leg, leg_name + "_tip")
		ik.set_target_node(leg, ik.get_path_to(targets[leg]))
		ik.set_pole_node(leg, ik.get_path_to(poles[leg]))


# --- visuals: plain meshes riding on BoneAttachment3D, so they follow the IK result ----------

func _add_body_visual(bone: int) -> void:
	if not layout.body_parts.is_empty() or not layout.body_visible:
		_add_body_parts(bone)
		return
	var body_mesh := SphereMesh.new()
	body_mesh.radius = layout.body_radius
	body_mesh.height = layout.body_radius * 1.2
	var attach := _attach(bone)
	attach.add_child(_mesh_instance(body_mesh, Transform3D(Basis.from_scale(layout.body_scale), Vector3.ZERO), layout.body_color))
	var front := -layout.body_radius * layout.body_scale.z * 0.86
	for side: float in [-1.0, 1.0]:
		_add_eye(attach, Vector3(0.13 * side, 0.1, front))


# A body built from the layout's parts (torso, head, …) instead of one sphere; eyes at eye_center.
func _add_body_parts(bone: int) -> void:
	var attach := _attach(bone)
	if layout.body_visible:
		for part in layout.body_parts:
			attach.add_child(_part_instance(part, layout.body_color))
	var eye_parent: Node3D = attach
	if not layout.head.is_empty():
		eye_parent = _add_head(attach)
	if layout.head.get("eyes", true):
		for side: float in [-1.0, 1.0]:
			_add_eye(eye_parent, layout.eye_center + Vector3(layout.eye_spacing * side, 0, 0))


# One body/head part from its layout description (sphere, capsule or box; see SpiderLayout).
func _part_instance(part: Dictionary, default_color: Color) -> MeshInstance3D:
	var mesh: PrimitiveMesh
	if part.shape == "capsule":
		var capsule := CapsuleMesh.new()
		capsule.radius = part.radius
		capsule.height = part.get("height", part.radius * 2.0)
		mesh = capsule
	elif part.shape == "box":
		var box := BoxMesh.new()
		box.size = part.size
		mesh = box
	else:
		var sphere := SphereMesh.new()
		sphere.radius = part.radius
		sphere.height = part.radius * 2.0
		mesh = sphere
	var basis := Basis.from_euler(part.get("rotation", Vector3.ZERO)).scaled(part.get("scale", Vector3.ONE))
	var instance := _mesh_instance(mesh, Transform3D(basis, part.get("at", Vector3.ZERO)), part.get("color", default_color))
	if part.has("emission"): # glows (a visor, a light)
		var material := instance.material_override as StandardMaterial3D
		material.emission_enabled = true
		material.emission = part.emission
		material.emission_energy_multiplier = part.get("glow", 2.0)
	return instance


# A detached head (a box) plus the coil of rings that will join it to the neck. Both are
# top_level — SpiderSprings places them every frame — but stay children of the body, so they
# hide with it. Starts at its rest spot.
func _add_head(attach: Node3D) -> Node3D:
	head = Node3D.new()
	head.name = "Head"
	attach.add_child(head)
	head.top_level = true
	head.global_transform = attach.global_transform * Transform3D(Basis.IDENTITY, layout.head.at)
	var box := BoxMesh.new()
	box.size = layout.head.size
	var head_color: Color = layout.head.get("color", layout.body_color)
	head.add_child(_mesh_instance(box, Transform3D.IDENTITY, head_color))
	for part: Dictionary in layout.head.get("parts", []): # visor, vents, ears… (head space)
		head.add_child(_part_instance(part, head_color))
	if layout.head.has("gun"):
		_add_gun(layout.head.gun)
	if not layout.head.get("coil", true):
		return head
	var ring := TorusMesh.new()
	ring.inner_radius = 0.035
	ring.outer_radius = 0.055
	for i in 7:
		var ring_instance := _mesh_instance(ring, Transform3D.IDENTITY, layout.leg_color)
		attach.add_child(ring_instance)
		ring_instance.top_level = true
		coil.append(ring_instance)
	return head


# Head gun: a pivot per barrel at its mount (head space) holding a square barrel that points
# forward (-Z) and a muzzle marker at its tip. SpiderGun slides the pivots back on recoil.
func _add_gun(gun: Dictionary) -> void:
	var length: float = gun.get("length", 0.3)
	var width: float = gun.get("width", 0.05)
	var color: Color = gun.get("color", Color(0.15, 0.15, 0.18))
	for mount: Vector3 in gun.barrels:
		var pivot := Node3D.new()
		pivot.name = "Barrel"
		pivot.position = mount
		head.add_child(pivot)
		var barrel := BoxMesh.new()
		barrel.size = Vector3(width, width, length)
		pivot.add_child(_mesh_instance(barrel, Transform3D(Basis.IDENTITY, Vector3(0, 0, -length * 0.5)), color))
		var muzzle := Marker3D.new()
		muzzle.name = "Muzzle"
		muzzle.position = Vector3(0, 0, -length)
		pivot.add_child(muzzle)
		gun_barrels.append(pivot)
		muzzles.append(muzzle)


# An eye = a pivot (aim it with look_at: its -Z faces the target) holding an eyeball and a pupil
# on the eyeball's -Z side. SpiderMood aims, blinks and colours them.
func _add_eye(parent: Node3D, at: Vector3) -> void:
	var pivot := Node3D.new()
	pivot.name = "Eye"
	pivot.position = at
	parent.add_child(pivot)
	var ball := SphereMesh.new()
	ball.radius = 0.075
	ball.height = 0.15
	var eyeball := _mesh_instance(ball, Transform3D.IDENTITY, Color(0.95, 0.95, 0.9))
	pivot.add_child(eyeball)
	var dot := SphereMesh.new()
	dot.radius = 0.04
	dot.height = 0.08
	var pupil := _mesh_instance(dot, Transform3D(Basis.IDENTITY, Vector3(0, 0, -0.055)), Color(0.05, 0.05, 0.08))
	pivot.add_child(pupil)
	eyes.append(pivot)
	eye_materials.append(eyeball.material_override as StandardMaterial3D)
	pupils.append(pupil)


# A capsule from `from` to `from + segment` in the bone's space (default: origin → child bone).
func _add_segment_visual(bone: int, segment: Vector3, radius: float, from := Vector3.ZERO) -> void:
	var capsule := CapsuleMesh.new()
	capsule.radius = radius
	capsule.height = segment.length() + radius * 2.0
	var y := segment.normalized() # capsules are built along +Y
	var helper := Vector3.FORWARD if absf(y.dot(Vector3.FORWARD)) < 0.9 else Vector3.RIGHT
	var x := y.cross(helper).normalized()
	var z := x.cross(y).normalized()
	_attach(bone).add_child(_mesh_instance(capsule, Transform3D(Basis(x, y, z), from + segment * 0.5), layout.leg_color))


# A square block along the bone (`segment` = child offset in bone space), shortened by `gap` at
# both ends so neighbouring blocks float apart — a limb of separate pieces, no joints drawn.
func _add_block_visual(bone: int, segment: Vector3, half_width: float, gap: float) -> void:
	_attach(bone).add_child(_block(Vector3.ZERO, segment, half_width, gap))


func _block(from: Vector3, segment: Vector3, half_width: float, gap: float) -> MeshInstance3D:
	var box := BoxMesh.new()
	box.size = Vector3(half_width * 2.0, maxf(segment.length() - gap * 2.0, 0.02), half_width * 2.0)
	var y := segment.normalized() # built along +Y
	var helper := Vector3.FORWARD if absf(y.dot(Vector3.FORWARD)) < 0.9 else Vector3.RIGHT
	var x := y.cross(helper).normalized()
	var z := x.cross(y).normalized()
	return _mesh_instance(box, Transform3D(Basis(x, y, z), from + segment * 0.5), layout.leg_color)


# A one-bone leg's visual: both segments as blocks in one node pivoting at the hip (body-space
# offsets), top_level so update_rigid_limbs() can aim it; a child of the body so it hides with it.
func _add_rigid_limb(leg: SpiderLayout.LegDef, body: Node3D) -> Node3D:
	var limb := Node3D.new()
	limb.name = leg.name + "_rigid"
	body.add_child(limb)
	limb.top_level = true
	var gap := leg.segment_gap if leg.square else 0.0
	limb.add_child(_block(Vector3.ZERO, leg.upper_vec(), leg.radius, gap))
	limb.add_child(_block(leg.upper_vec(), leg.lower_vec(), leg.radius, gap))
	return limb


# Scale by `factor` along `axis` only (1 across it): stretches a limb without fattening it.
static func _stretch_along(axis: Vector3, factor: float) -> Basis:
	var a := axis.normalized()
	var k := factor - 1.0
	return Basis(Vector3.RIGHT + a * a.x * k, Vector3.UP + a * a.y * k, Vector3.BACK + a * a.z * k)


# An arm's claw: two fingers splayed in a V past the end of the forearm. They ride on the
# forearm bone (`lower`), so they swing with it.
func _add_pincer(lower: int, leg: SpiderLayout.LegDef) -> void:
	var forearm := leg.lower_vec()
	var along := forearm.normalized()
	var side := along.cross(Vector3.UP).normalized()
	for spread: float in [-0.45, 0.45]:
		var finger := (along * cos(spread) + side * sin(spread)) * 0.2
		_add_segment_visual(lower, finger, leg.radius * 0.5, forearm)


func _attach(bone: int) -> BoneAttachment3D:
	var attach := BoneAttachment3D.new()
	attach.name = skeleton.get_bone_name(bone) + "_attach"
	skeleton.add_child(attach)
	attach.bone_idx = bone
	return attach


func _mesh_instance(mesh: Mesh, xform: Transform3D, color: Color) -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.6
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	instance.transform = xform
	return instance
