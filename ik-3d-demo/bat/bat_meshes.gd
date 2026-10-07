## Mesh and material helpers shared by the bat rig, its wings and the swarm.
class_name BatMeshes
extends RefCounted


static func material(color: Color, emission := 0.0, double_sided := false) -> StandardMaterial3D:
	var result := StandardMaterial3D.new()
	result.albedo_color = color
	result.roughness = 0.8
	if emission > 0.0:
		result.emission_enabled = true
		result.emission = color
		result.emission_energy_multiplier = emission
	if double_sided:
		result.cull_mode = BaseMaterial3D.CULL_DISABLED
	return result


static func instance(mesh: Mesh, xform: Transform3D, color: Color, emission := 0.0) -> MeshInstance3D:
	var result := MeshInstance3D.new()
	result.mesh = mesh
	result.material_override = material(color, emission)
	result.transform = xform
	return result


## A unit sphere squashed to `size` (radii) at `at`.
static func blob(size: Vector3, at: Vector3, color: Color, emission := 0.0) -> MeshInstance3D:
	return instance(sphere(1.0), Transform3D(Basis.from_scale(size), at), color, emission)


static func sphere(radius: float) -> SphereMesh:
	var result := SphereMesh.new()
	result.radius = radius
	result.height = radius * 2.0
	result.radial_segments = 16
	result.rings = 8
	return result


static func cone(radius: float, height: float) -> CylinderMesh:
	var result := CylinderMesh.new()
	result.top_radius = 0.0
	result.bottom_radius = radius
	result.height = height
	result.radial_segments = 8
	result.rings = 1
	return result


## A capsule lying along `direction` from the origin, as long as it (for limb visuals).
static func limb(direction: Vector3, radius: float, color: Color) -> MeshInstance3D:
	var capsule := CapsuleMesh.new()
	capsule.radius = radius
	capsule.height = direction.length() + radius * 2.0
	capsule.radial_segments = 8
	capsule.rings = 2
	return instance(capsule, Transform3D(basis_along(direction.normalized()), direction * 0.5), color)


## A basis whose +Y points along `y`.
static func basis_along(y: Vector3) -> Basis:
	var helper := Vector3.FORWARD if absf(y.dot(Vector3.FORWARD)) < 0.9 else Vector3.RIGHT
	var x := y.cross(helper).normalized()
	return Basis(x, y, x.cross(y))


## A flat triangle fan from `centre` round the outline `points`. Each triangle faces the side
## `up` points to (Godot fronts are clockwise), so lighting is right on a double-sided material.
static func fan(centre: Vector3, points: PackedVector3Array, up: Vector3) -> ArrayMesh:
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	for k in points.size():
		var a := points[k]
		var b := points[(k + 1) % points.size()]
		var normal := (a - centre).cross(b - centre)
		if normal.length_squared() < 1e-12:
			continue
		if normal.dot(up) < 0.0:
			var swap := a
			a = b
			b = swap
			normal = -normal
		vertices.append_array([centre, b, a])
		normal = normal.normalized()
		normals.append_array([normal, normal, normal])
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh
