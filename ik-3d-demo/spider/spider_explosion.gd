## A spider blowing apart: a fireball, a flash of light, and the spider's own pieces — every leg
## segment and chunks of its body, copied from where they are right now — flung out as physics
## debris that tumbles, bounces, then shrinks away. The spider itself is not touched (hide it).
##
## Debris sits on physics layer DEBRIS_LAYER and only collides with the ground (layer 1), so
## the spider's ground raycasts (Spider.ground_mask) never land feet on flying pieces.
class_name SpiderExplosion
extends Node3D

const DEBRIS_LAYER := 2
const GROUND_LAYER := 1
const LIFETIME := 3.0 ## seconds until everything is gone
const FADE_TIME := 0.5 ## debris shrinks away over the last this-many seconds

var _center: Vector3
var _rng := RandomNumberGenerator.new()


## Blow `spider` up where it stands. The explosion lives next to it in the scene and frees itself.
static func spawn(spider: Spider, power := 1.0) -> SpiderExplosion:
	var explosion := SpiderExplosion.new()
	explosion.name = "SpiderExplosion"
	spider.get_parent().add_child(explosion)
	explosion.top_level = true
	explosion._center = spider.rig.skeleton.global_position
	explosion._fireball(power)
	explosion._flash(power)
	explosion._leg_debris(spider, power)
	explosion._body_debris(spider, power)
	explosion.get_tree().create_timer(LIFETIME).timeout.connect(explosion.queue_free)
	return explosion


# --- fire + light ----------------------------------------------------------------------------

# Two glowing balls (a hot core inside a bigger orange one) that swell and fade fast.
func _fireball(power: float) -> void:
	for layer in [[Color(1.0, 0.55, 0.15), 2.6, 0.45], [Color(1.0, 0.95, 0.7), 1.4, 0.25]]:
		var material := StandardMaterial3D.new()
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		material.albedo_color = layer[0]
		var sphere := SphereMesh.new()
		sphere.radius = 0.5
		sphere.height = 1.0
		var ball := MeshInstance3D.new()
		ball.mesh = sphere
		ball.material_override = material
		ball.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(ball)
		ball.global_position = _center
		ball.scale = Vector3.ONE * 0.3
		var size: float = layer[1] * power
		var time: float = layer[2]
		var tween := ball.create_tween().set_parallel()
		tween.tween_property(ball, "scale", Vector3.ONE * size, time).set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_EXPO)
		tween.tween_property(material, "albedo_color:a", 0.0, time * 1.4).set_ease(Tween.EASE_IN)
		tween.chain().tween_callback(ball.queue_free)


func _flash(power: float) -> void:
	var light := OmniLight3D.new()
	light.light_color = Color(1.0, 0.6, 0.25)
	light.light_energy = 12.0 * power
	light.omni_range = 9.0 * power
	add_child(light)
	light.global_position = _center + Vector3.UP * 0.3
	light.create_tween().tween_property(light, "light_energy", 0.0, 0.45).set_ease(Tween.EASE_OUT)


# --- debris ----------------------------------------------------------------------------------

# Each leg segment (hip → knee, knee → foot) becomes a capsule exactly where it is now.
func _leg_debris(spider: Spider, power: float) -> void:
	var skeleton := spider.rig.skeleton
	for leg in spider.rig.layout.legs.size():
		var definition := spider.rig.layout.legs[leg]
		var limb := spider.rig.rigid_limbs[leg]
		if limb != null: # a one-bone leg: its blocks fly off as they are (stretch baked into size)
			for block: MeshInstance3D in limb.get_children():
				var xform := block.global_transform
				var size := (block.mesh as BoxMesh).size * xform.basis.get_scale()
				_add_box(xform.orthonormalized(), size, spider.rig.layout.leg_color, power)
			continue
		var joints: Array[Vector3] = []
		for suffix in ["_upper", "_lower", "_tip"]:
			var bone := skeleton.find_bone(definition.name + suffix)
			joints.append(skeleton.global_transform * skeleton.get_bone_global_pose(bone).origin)
		var color := spider.rig.layout.leg_color
		_add_segment(joints[0], joints[1], definition.radius, color, power)
		_add_segment(joints[1], joints[2], definition.radius, color, power)


# The body becomes chunks: its parts if the layout has them (torso, head…), else three pieces
# of the body sphere.
func _body_debris(spider: Spider, power: float) -> void:
	var layout := spider.rig.layout
	var body := spider.rig.skeleton.global_transform
	if not layout.body_visible: # nothing to break but the head
		if spider.rig.head != null:
			_add_box(spider.rig.head.global_transform, layout.head.size, layout.head.get("color", layout.body_color), power)
		return
	if layout.body_parts.is_empty():
		var chunk := layout.body_radius * 0.6
		for i in 3:
			var offset := Vector3(_rng.randf_range(-1, 1), _rng.randf_range(0, 0.5), _rng.randf_range(-1, 1)) * chunk * 0.5
			_add_ball(body * offset, chunk, layout.body_color, power)
		return
	for part in layout.body_parts:
		var where: Vector3 = body * part.get("at", Vector3.ZERO)
		var color: Color = part.get("color", layout.body_color)
		if part.shape == "box":
			_add_box(Transform3D(body.basis, where), part.size, color, power)
			continue
		var scale: Vector3 = part.get("scale", Vector3.ONE)
		var radius: float = part.radius * (scale.x + scale.y + scale.z) / 3.0
		_add_ball(where, radius, color, power)
	if spider.rig.head != null: # a detached head flies off too
		_add_box(spider.rig.head.global_transform, layout.head.size, layout.head.get("color", layout.body_color), power)


func _add_segment(from: Vector3, to: Vector3, radius: float, color: Color, power: float) -> void:
	var length := from.distance_to(to)
	if length < 0.01:
		return
	var shape := CapsuleShape3D.new()
	shape.radius = radius
	shape.height = length + radius * 2.0
	var mesh := CapsuleMesh.new()
	mesh.radius = radius
	mesh.height = shape.height
	var y := (to - from) / length # capsules are built along +Y
	var helper := Vector3.FORWARD if absf(y.dot(Vector3.FORWARD)) < 0.9 else Vector3.RIGHT
	var x := y.cross(helper).normalized()
	_launch(_make_piece(shape, mesh, color), Transform3D(Basis(x, y, x.cross(y)), (from + to) * 0.5), power)


func _add_ball(where: Vector3, radius: float, color: Color, power: float) -> void:
	var shape := SphereShape3D.new()
	shape.radius = radius
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	_launch(_make_piece(shape, mesh, color), Transform3D(Basis.IDENTITY, where), power * 0.7)


func _add_box(xform: Transform3D, size: Vector3, color: Color, power: float) -> void:
	var shape := BoxShape3D.new()
	shape.size = size
	var mesh := BoxMesh.new()
	mesh.size = size
	_launch(_make_piece(shape, mesh, color), xform, power * 0.7)


func _make_piece(shape: Shape3D, mesh: Mesh, color: Color) -> RigidBody3D:
	var piece := RigidBody3D.new()
	piece.collision_layer = 1 << (DEBRIS_LAYER - 1)
	piece.collision_mask = 1 << (GROUND_LAYER - 1)
	var bounce := PhysicsMaterial.new()
	bounce.bounce = 0.35
	bounce.friction = 0.8
	piece.physics_material_override = bounce
	var collision := CollisionShape3D.new()
	collision.shape = shape
	piece.add_child(collision)
	var material := StandardMaterial3D.new()
	material.albedo_color = color.darkened(0.25) # scorched
	var visual := MeshInstance3D.new()
	visual.mesh = mesh
	visual.material_override = material
	piece.add_child(visual)
	return piece


# Fling a piece out from the centre and up, spinning; shrink it away near the end.
func _launch(piece: RigidBody3D, xform: Transform3D, power: float) -> void:
	add_child(piece)
	piece.global_transform = xform
	var outward := (xform.origin - _center) * Vector3(1, 0, 1)
	if outward.length() < 0.05:
		outward = Vector3(_rng.randf_range(-1, 1), 0, _rng.randf_range(-1, 1))
	var direction := (outward.normalized() + Vector3.UP * _rng.randf_range(0.8, 1.6)).normalized()
	piece.linear_velocity = direction * _rng.randf_range(3.5, 7.0) * power
	piece.angular_velocity = Vector3(_rng.randf_range(-1, 1), _rng.randf_range(-1, 1), _rng.randf_range(-1, 1)) * 12.0
	var tween := piece.create_tween()
	tween.tween_interval(LIFETIME - FADE_TIME)
	tween.tween_property(piece.get_child(1), "scale", Vector3.ONE * 0.01, FADE_TIME)
