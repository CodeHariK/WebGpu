## The skyship demo's little mesh kit: materials (metal, glow, unshaded see-through) and code-made
## primitives, plus `beam()` to stretch a cylinder between two points. Everything in the demo is
## built from these, so the look stays consistent and no file repeats material setup.
class_name ShipKit
extends RefCounted

const RED := Color(1.0, 0.12, 0.05)
const HOT := Color(1.0, 0.5, 0.15)
const CYAN := Color(0.3, 0.9, 1.0)
const STEEL := Color(0.28, 0.3, 0.34)
const DARK := Color(0.12, 0.13, 0.15)


static func metal(color := STEEL, rough := 0.45) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = 0.7
	material.roughness = rough
	return material


## Lit by itself (emission): engines, eyes, crystals.
static func glow(color: Color, energy := 2.5) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = energy
	return material


## Unshaded and see-through (alpha in `color.a`): telegraphs, beams, the shield bubble.
static func ghost(color: Color, both_sides := false) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = color
	material.cull_mode = BaseMaterial3D.CULL_DISABLED if both_sides else BaseMaterial3D.CULL_BACK
	return material


static func add(parent: Node3D, mesh: Mesh, material: Material, at := Vector3.ZERO) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	instance.position = at
	parent.add_child(instance)
	return instance


static func box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


static func sphere(radius: float, segments := 24) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	mesh.radial_segments = segments
	mesh.rings = segments / 2
	return mesh


static func cylinder(top: float, bottom: float, height: float, sides := 16) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top
	mesh.bottom_radius = bottom
	mesh.height = height
	mesh.radial_segments = sides
	return mesh


## A flat ring on the ground (a torus squashed flat): blast markers, the clock's hub.
static func ring(inner: float, outer: float) -> TorusMesh:
	var mesh := TorusMesh.new()
	mesh.inner_radius = inner
	mesh.outer_radius = outer
	mesh.rings = 48
	mesh.ring_segments = 4
	return mesh


## Stretch `instance` (a unit-height cylinder along +Y) from `from` to `to`.
static func beam(instance: Node3D, from: Vector3, to: Vector3, width := 1.0) -> void:
	var along := to - from
	var length := along.length()
	if length < 1e-4:
		instance.visible = false
		return
	var y := along / length
	var x := y.cross(Vector3.UP if absf(y.y) < 0.99 else Vector3.RIGHT).normalized()
	var z := x.cross(y)
	instance.global_transform = Transform3D(Basis(x * width, y * length, z * width), (from + to) * 0.5)


## Where a point is on the arena floor (y = 0).
static func flat(point: Vector3) -> Vector3:
	return Vector3(point.x, 0.0, point.z)
