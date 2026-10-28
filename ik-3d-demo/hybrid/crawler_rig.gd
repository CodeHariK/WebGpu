## The crawler's look: one rigid mesh per bone, no Skeleton3D, no IK modifier.
##
##   limbs   four LimbChains (limbs/limb_chain.gd: exact two-bone IK, law of cosines, the
##           knee/elbow bending toward a pole). Root on the shoulder/hip, target the step's spot
##           lifted off its surface, pole the layout's magnet offset (cultist). The upper and
##           lower segments, their joints, the neck and the spine are all drawn by one
##           LimbRenderer (3 MultiMesh draw calls for everything).
##   errors  after solving, how far each limb's tip missed its target (chain.error()), handed to
##           the gait so it re-steps limbs that couldn't reach (the cultist's IK feedback)
##   hands   flat boxes on the tips, turned like the cultist's: on a surface, palm/sole flat on
##   feet    it and pointing away from the body; in the air, along the bend
##   chest   the body's own transform; the pelvis follows it with a lag (slerp), so the spine
##   pelvis  flexes on turns and the hips swing round after the shoulders
##   head    on the neck, turned toward `watch` through a spring (SecondOrder), limited to
##           head_limit from straight ahead; two glowing eyes
##
## Everything is in world space (top_level), updated once per physics tick by Crawler.
class_name CrawlerRig
extends Node3D

const SKIN := Color(0.56, 0.45, 0.4)
const DARK := Color(0.32, 0.26, 0.24)
const EYE := Color(0.45, 0.75, 1.0)
const PELVIS_OFFSET := Vector3(0.0, -0.02, 0.3) ## body frame
const NECK_BASE := Vector3(0.0, 0.05, -0.36) ## body frame
const HEAD_OFFSET := Vector3(0.0, 0.1, -0.2) ## from the neck base, body frame

var pelvis_lag := 7.0 ## 1/s: how quickly the pelvis turns after the chest
var head_limit := 1.3 ## radians from straight ahead

var chains: Array[LimbChain] = []
var errors := PackedFloat32Array([0.0, 0.0, 0.0, 0.0])
var renderer := LimbRenderer.new()

var _neck := LimbChain.new(PackedFloat32Array([0.22]), Vector3.ZERO, LimbChain.Solver.FK)
var _spine := LimbChain.new(PackedFloat32Array([0.6]), Vector3.ZERO, LimbChain.Solver.FK)
var _chest := MeshInstance3D.new()
var _pelvis := MeshInstance3D.new()
var _head := MeshInstance3D.new()
var _ends: Array[MeshInstance3D] = []
var _pelvis_basis := Basis.IDENTITY
var _look: SecondOrder


func _ready() -> void:
	top_level = true
	for limb in CrawlerLayout.COUNT:
		var lengths: Vector2 = CrawlerLayout.LENGTHS[limb]
		var chain := LimbChain.new(PackedFloat32Array([lengths.x, lengths.y]))
		chain.radius = 0.05 if CrawlerLayout.is_foot(limb) else 0.045
		chain.taper = 0.6
		chain.color = SKIN
		chains.append(chain)
		renderer.add(chain)
		_ends.append(_box(Vector3(0.1, 0.035, 0.18) if CrawlerLayout.is_foot(limb) else Vector3(0.11, 0.03, 0.15), SKIN))
	for chain in [_spine, _neck]:
		chain.radius = 0.075 if chain == _spine else 0.05
		chain.color = DARK
		renderer.add(chain)
	renderer.name = "Limbs"
	add_child(renderer)
	_chest = _box(Vector3(0.38, 0.2, 0.42), DARK)
	_pelvis = _box(Vector3(0.32, 0.17, 0.3), DARK)
	_head = _make_head()
	_look = SecondOrder.new(3.0, 0.7, 0.0, Vector3.FORWARD)


## Pose everything for this frame. `body`: the posed body; `steps`: the gait's hands and feet;
## `watch`: a world point to watch (null: straight ahead).
func update(delta: float, body: Transform3D, steps: Array[CrawlerStep], watch: Variant) -> void:
	_pelvis_basis = Basis(_pelvis_basis.get_rotation_quaternion().slerp(body.basis.get_rotation_quaternion(), minf(delta * pelvis_lag, 1.0)))
	var pelvis := Transform3D(_pelvis_basis, body * PELVIS_OFFSET)
	_chest.global_transform = body
	_pelvis.global_transform = pelvis
	for limb in CrawlerLayout.COUNT:
		_pose_limb(limb, body, pelvis, steps[limb])
	_spine.points[0] = body.origin
	_spine.points[1] = pelvis.origin
	_pose_head(delta, body, watch)
	renderer.draw()


func _pose_limb(limb: int, body: Transform3D, pelvis: Transform3D, step: CrawlerStep) -> void:
	var chain := chains[limb]
	var root_local: Vector3 = CrawlerLayout.ROOTS[limb]
	chain.root = pelvis * (root_local - PELVIS_OFFSET) if CrawlerLayout.is_foot(limb) else body * root_local
	chain.target = step.position + step.normal * CrawlerLayout.LIFT[limb]
	chain.pole = (chain.root + chain.target) * 0.5 + body.basis * CrawlerLayout.POLES[limb]
	chain.solve()
	errors[limb] = chain.error()
	var tip := chain.tip()
	var facing: Vector3
	var up: Vector3
	if step.airborne:
		facing = (tip - body.origin) * 0.5 + body.basis * CrawlerLayout.POLES[limb]
		up = body.basis.y
	else:
		up = step.normal
		facing = tip - body.origin
		facing -= up * facing.dot(up)
	var basis := SurfaceWalker.surface_basis(up, facing if facing.length() > 1e-3 else -body.basis.z)
	_ends[limb].global_transform = Transform3D(basis, tip + basis * Vector3(0.0, 0.0, -0.06))


func _pose_head(delta: float, body: Transform3D, watch: Variant) -> void:
	var base := body * NECK_BASE
	var head_at := base + body.basis * HEAD_OFFSET
	var forward := -body.basis.z
	var want := forward
	if watch is Vector3:
		want = ((watch as Vector3) - head_at).normalized()
		var angle := forward.angle_to(want)
		if angle > head_limit:
			want = forward.slerp(want, head_limit / angle)
	var look := _look.update(delta, want)
	if look.length() < 1e-3:
		look = forward
	_head.global_transform = Transform3D(SurfaceWalker.surface_basis(body.basis.y, look), head_at)
	_neck.points[0] = base
	_neck.points[1] = head_at


func _make_head() -> MeshInstance3D:
	var head := MeshInstance3D.new()
	var skull := SphereMesh.new()
	skull.radius = 0.12
	skull.height = 0.22
	skull.radial_segments = 12
	skull.rings = 6
	head.mesh = skull
	head.material_override = _material(DARK)
	add_child(head)
	for side in [-1.0, 1.0]:
		var eye := MeshInstance3D.new()
		var ball := SphereMesh.new()
		ball.radius = 0.025
		ball.height = 0.05
		eye.mesh = ball
		var glow := _material(Color.BLACK)
		glow.emission_enabled = true
		glow.emission = EYE
		glow.emission_energy_multiplier = 3.0
		eye.material_override = glow
		eye.position = Vector3(0.045 * side, 0.02, -0.105)
		head.add_child(eye)
	return head


func _box(size: Vector3, color: Color) -> MeshInstance3D:
	var mesh := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = size
	mesh.mesh = box
	mesh.material_override = _material(color)
	add_child(mesh)
	return mesh


func _material(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.75
	return material
