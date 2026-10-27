## Bullet impacts: a dark bullet hole on the surface and a puff of dust and sparks.
##
## Pooled: HOLES quads and PUFFS particle emitters are made once; each hit reuses the oldest.
## The holes are tiny quads (2 triangles, one shared material, a radial gradient texture made
## in code, so no texture file). They're parented to what was hit, so a hole on a crate moves
## with the crate. The puffs are one-shot CPUParticles3D (fine on every mobile GPU).
extends Node3D

const HOLES := 32
const PUFFS := 6

var _holes: Array[MeshInstance3D] = []
var _puffs: Array[CPUParticles3D] = []
var _next_hole := 0
var _next_puff := 0


func _ready() -> void:
	var quad := QuadMesh.new()
	quad.size = Vector2(0.1, 0.1)
	quad.material = _hole_material()
	for i in HOLES:
		var hole := MeshInstance3D.new()
		hole.mesh = quad
		hole.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		hole.visible = false
		add_child(hole)
		_holes.append(hole)
	for i in PUFFS:
		var puff := _make_puff()
		add_child(puff)
		_puffs.append(puff)


## Show a hit at `point` on a surface facing `normal`. `collider` (if a Node3D) carries the hole.
func spawn(collider: Object, point: Vector3, normal: Vector3) -> void:
	var facing := _facing(normal)
	var hole := _holes[_next_hole]
	_next_hole = (_next_hole + 1) % HOLES
	var parent: Node = collider if collider is Node3D else self
	if hole.get_parent() != parent:
		hole.reparent(parent, false)
	hole.global_transform = Transform3D(facing.rotated(normal, randf() * TAU), point + normal * 0.004)
	hole.visible = true
	var puff := _puffs[_next_puff]
	_next_puff = (_next_puff + 1) % PUFFS
	puff.global_transform = Transform3D(facing, point + normal * 0.02)
	puff.restart()


## A basis whose +Z points along `normal` (a QuadMesh faces +Z).
func _facing(normal: Vector3) -> Basis:
	var up := Vector3.UP if absf(normal.y) < 0.95 else Vector3.FORWARD
	var x := up.cross(normal).normalized()
	return Basis(x, normal.cross(x), normal)


func _hole_material() -> StandardMaterial3D:
	var gradient := Gradient.new()
	gradient.set_color(0, Color(0.02, 0.015, 0.01, 1.0))
	gradient.set_color(1, Color(0.05, 0.04, 0.03, 0.0))
	gradient.add_point(0.35, Color(0.02, 0.015, 0.01, 1.0))
	gradient.add_point(0.55, Color(0.12, 0.09, 0.06, 0.6))
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = _radial(gradient, 32)
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	return mat


## A round soft texture from a gradient (centre → edge), made in code: no texture file.
static func _radial(gradient: Gradient, size: int) -> GradientTexture2D:
	var texture := GradientTexture2D.new()
	texture.gradient = gradient
	texture.fill = GradientTexture2D.FILL_RADIAL
	texture.fill_from = Vector2(0.5, 0.5)
	texture.fill_to = Vector2(1.0, 0.5)
	texture.width = size
	texture.height = size
	return texture


## Grey dust and a few orange sparks flying out along the normal (+Z of the emitter).
func _make_puff() -> CPUParticles3D:
	var p := CPUParticles3D.new()
	p.emitting = false
	p.one_shot = true
	p.explosiveness = 1.0
	p.amount = 10
	p.lifetime = 0.5
	p.local_coords = false
	p.direction = Vector3(0, 0, 1)
	p.spread = 35.0
	p.initial_velocity_min = 1.0
	p.initial_velocity_max = 3.5
	p.gravity = Vector3(0, -6.0, 0)
	p.damping_min = 2.0
	p.damping_max = 4.0
	p.scale_amount_min = 0.5
	p.scale_amount_max = 1.5
	var curve := Curve.new()
	curve.add_point(Vector2(0, 1))
	curve.add_point(Vector2(1, 0))
	p.scale_amount_curve = curve
	var colors := Gradient.new()
	colors.set_color(0, Color(1.0, 0.7, 0.3))
	colors.set_color(1, Color(0.45, 0.4, 0.35, 0.0))
	p.color_ramp = colors
	var mesh := QuadMesh.new()
	mesh.size = Vector2(0.025, 0.025)
	var soft := Gradient.new()
	soft.set_color(0, Color.WHITE)
	soft.set_color(1, Color(1, 1, 1, 0))
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = _radial(soft, 16)
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.vertex_color_use_as_albedo = true
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	mesh.material = mat
	p.mesh = mesh
	return p
