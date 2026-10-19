## Props for the mood demo, all from primitive meshes with flat, matte, soft-shaded materials
## (Lambert wrap diffuse, no specular, no metal: the lighting and fog make the mood, not texture
## detail). Static helpers; each returns the prop's root node, already placed.
class_name MoodProps
extends RefCounted

const PINE := Color(0.11, 0.15, 0.13)
const DEAD_WOOD := Color(0.17, 0.12, 0.11)
const PLANK := Color(0.42, 0.27, 0.18)
const POST := Color(0.16, 0.11, 0.1)
const STONE := Color(0.3, 0.27, 0.33)
const PUMPKIN := Color(0.95, 0.45, 0.08)
const FLAME := Color(1.0, 0.55, 0.15)

static var _materials := {}


static func material(color: Color, glow := 0.0, rim := 0.0) -> StandardMaterial3D:
	var key := "%s/%.2f/%.2f" % [color.to_html(), glow, rim]
	if not _materials.has(key):
		var m := StandardMaterial3D.new()
		m.albedo_color = color
		m.roughness = 1.0
		m.diffuse_mode = BaseMaterial3D.DIFFUSE_LAMBERT_WRAP # soft, painted falloff
		m.specular_mode = BaseMaterial3D.SPECULAR_DISABLED
		if glow > 0.0:
			m.emission_enabled = true
			m.emission = color
			m.emission_energy_multiplier = glow
		if rim > 0.0: # a bright edge against the fog
			m.rim_enabled = true
			m.rim = rim
			m.rim_tint = 0.3
		_materials[key] = m
	return _materials[key]


static func mesh(parent: Node3D, shape: PrimitiveMesh, at: Vector3, color: Color, rotation := Vector3.ZERO, glow := 0.0) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = shape
	instance.material_override = material(color, glow)
	instance.transform = Transform3D(Basis.from_euler(rotation), at)
	parent.add_child(instance)
	return instance


static func box(parent: Node3D, size: Vector3, at: Vector3, color: Color, rotation := Vector3.ZERO) -> MeshInstance3D:
	var shape := BoxMesh.new()
	shape.size = size
	return mesh(parent, shape, at, color, rotation)


static func cone(parent: Node3D, radius: float, height: float, at: Vector3, color: Color) -> MeshInstance3D:
	var shape := CylinderMesh.new()
	shape.top_radius = 0.0
	shape.bottom_radius = radius
	shape.height = height
	shape.radial_segments = 7
	shape.rings = 1
	return mesh(parent, shape, at, color)


static func _root(parent: Node3D, at: Vector3, yaw := 0.0) -> Node3D:
	var root := Node3D.new()
	root.position = at
	root.rotation.y = yaw
	parent.add_child(root)
	return root


## A low-poly pine: a trunk and three stacked cones.
static func pine(parent: Node3D, at: Vector3, height: float) -> Node3D:
	var root := _root(parent, at, randf() * TAU)
	box(root, Vector3(0.25, height * 0.3, 0.25), Vector3(0, height * 0.15, 0), DEAD_WOOD)
	for i in 3:
		var t := float(i) / 3.0
		cone(root, height * lerpf(0.3, 0.14, t), height * 0.45, Vector3(0, height * lerpf(0.4, 0.82, t), 0), PINE)
	return root


## A dead tree: a leaning trunk with a few crooked bare branches.
static func dead_tree(parent: Node3D, at: Vector3, height: float) -> Node3D:
	var root := _root(parent, at, randf() * TAU)
	var lean := Vector3(randf_range(-0.15, 0.15), 0, randf_range(-0.15, 0.15))
	box(root, Vector3(0.22, height, 0.22), Vector3(0, height * 0.5, 0), DEAD_WOOD, lean)
	for i in 4:
		var y := height * randf_range(0.45, 0.9)
		var side := 1.0 if i % 2 == 0 else -1.0
		var length := height * randf_range(0.2, 0.35)
		box(root, Vector3(length, 0.08, 0.08), Vector3(side * length * 0.45, y, randf_range(-0.2, 0.2)), DEAD_WOOD, Vector3(0, randf() * TAU, side * randf_range(0.3, 0.7)))
	return root


## A lantern post: post, arm, a lantern box with an emissive flame and a short-range orange light.
## Returns [root, light, flame material].
static func lantern(parent: Node3D, at: Vector3, yaw: float) -> Array:
	var root := _root(parent, at, yaw)
	box(root, Vector3(0.12, 1.9, 0.12), Vector3(0, 0.95, 0), POST)
	box(root, Vector3(0.6, 0.07, 0.07), Vector3(0.28, 1.8, 0), POST)
	box(root, Vector3(0.22, 0.28, 0.22), Vector3(0.52, 1.56, 0), POST)
	var flame_shape := SphereMesh.new()
	flame_shape.radius = 0.07
	flame_shape.height = 0.16
	var flame := StandardMaterial3D.new() # its own: the energy flickers per lantern
	flame.albedo_color = FLAME
	flame.emission_enabled = true
	flame.emission = FLAME
	flame.emission_energy_multiplier = 4.0
	flame.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var instance := MeshInstance3D.new()
	instance.mesh = flame_shape
	instance.material_override = flame
	instance.position = Vector3(0.52, 1.55, 0)
	root.add_child(instance)
	var light := OmniLight3D.new()
	light.light_color = Color(1.0, 0.55, 0.25)
	light.omni_range = 4.5
	light.shadow_enabled = false # cheap: only the moon casts shadows
	light.position = Vector3(0.52, 1.5, 0)
	root.add_child(light)
	return [root, light, flame]


## A small pumpkin, sometimes carved with a glowing face.
static func pumpkin(parent: Node3D, at: Vector3, size: float, carved: bool) -> Node3D:
	var root := _root(parent, at, randf() * TAU)
	var shape := SphereMesh.new()
	shape.radius = size
	shape.height = size * 1.4
	shape.radial_segments = 10
	shape.rings = 6
	mesh(root, shape, Vector3(0, size * 0.65, 0), PUMPKIN)
	box(root, Vector3(0.05, 0.14, 0.05) * size * 4.0, Vector3(0, size * 1.4, 0), Color(0.25, 0.3, 0.1))
	if carved:
		for x: float in [-0.35, 0.35]:
			box(root, Vector3(0.22, 0.18, 0.05) * size, Vector3(x * size, size * 0.8, -size * 0.98), FLAME).material_override = material(FLAME, 3.0)
		box(root, Vector3(0.6, 0.12, 0.05) * size, Vector3(0, size * 0.45, -size * 0.98), FLAME).material_override = material(FLAME, 3.0)
	return root


## A stone tower: stacked uneven blocks, a lantern-lit top.
static func tower(parent: Node3D, at: Vector3, height: float) -> Node3D:
	var root := _root(parent, at, randf() * TAU)
	var y := 0.0
	while y < height:
		var block := randf_range(1.2, 2.2)
		var width := randf_range(1.6, 2.1)
		box(root, Vector3(width, block, width), Vector3(randf_range(-0.1, 0.1), y + block * 0.5, randf_range(-0.1, 0.1)), STONE, Vector3(0, randf_range(-0.1, 0.1), 0))
		y += block
	box(root, Vector3(2.4, 0.3, 2.4), Vector3(0, y + 0.15, 0), POST)
	return root


## A fence run from `a` to `b` (ground points): crooked posts and two rails.
static func fence(parent: Node3D, a: Vector3, b: Vector3) -> void:
	var count := maxi(int(a.distance_to(b) / 1.6), 1)
	for i in count + 1:
		var at := a.lerp(b, float(i) / count)
		box(parent, Vector3(0.12, 1.1, 0.12), at + Vector3.UP * 0.5, POST, Vector3(randf_range(-0.12, 0.12), 0, randf_range(-0.12, 0.12)))
	var middle := a.lerp(b, 0.5)
	var yaw := atan2(b.x - a.x, b.z - a.z)
	for h: float in [0.45, 0.85]:
		box(parent, Vector3(0.06, 0.1, a.distance_to(b)), middle + Vector3.UP * h, POST, Vector3(randf_range(-0.04, 0.04), yaw, 0))
