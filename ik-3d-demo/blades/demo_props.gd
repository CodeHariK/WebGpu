## The demo scene's simple meshes: ground, grapple posts, capsules and the blade itself.
class_name DemoProps
extends RefCounted


static func mesh_instance(
		parent: Node3D,
		mesh: Mesh,
		color: Color,
		glow := false
) -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	if glow:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = 2.5
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	parent.add_child(instance)
	return instance


static func capsule(
		radius: float,
		height: float
) -> Mesh:
	var mesh := CapsuleMesh.new()
	mesh.radius = radius
	mesh.height = height
	return mesh


## A flat blade pointing along −Z (look_at's forward).
static func blade_mesh() -> Mesh:
	var mesh := PrismMesh.new()
	mesh.size = Vector3(0.18, 0.7, 0.05)
	var turned := ArrayMesh.new()
	var arrays := mesh.get_mesh_arrays()
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
	var turn := Basis(Vector3.RIGHT, -PI / 2.0) # prism tip from +Y to −Z
	for i in vertices.size():
		vertices[i] = turn * vertices[i]
		normals[i] = turn * normals[i]
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TANGENT] = null
	turned.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return turned


## Grapple posts: a pole from the ground up to a glowing ring at each top.
static func add_posts(
		parent: Node3D,
		tops: Array
) -> void:
	for top: Vector3 in tops:
		var pole := CylinderMesh.new()
		pole.top_radius = 0.12
		pole.bottom_radius = 0.18
		pole.height = top.y
		var pole_instance := mesh_instance(parent, pole, Color(0.4, 0.36, 0.33))
		pole_instance.position = Vector3(top.x, top.y * 0.5, top.z)
		var ring := TorusMesh.new()
		ring.inner_radius = 0.28
		ring.outer_radius = 0.4
		var ring_instance := mesh_instance(parent, ring, Color(0.3, 0.9, 1.0), true)
		ring_instance.position = top + Vector3(0, 0.3, 0)
		ring_instance.rotation.x = PI / 2.0


static func add_ground(parent: Node3D) -> MeshInstance3D:
	var mesh := PlaneMesh.new()
	mesh.size = Vector2(60, 60)
	var ground := MeshInstance3D.new()
	ground.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.3, 0.28, 0.27)
	ground.material_override = material
	parent.add_child(ground)
	return ground
