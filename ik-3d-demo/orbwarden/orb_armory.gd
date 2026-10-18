## Builds the Orb Warden's weak spots on its (re)built body and registers them with OrbParts:
##   4 power cores    glowing cells in housings around the sphere between the legs
##   2 missile pods   launch boxes on the upper shoulders, four glowing tubes each (OrbMissiles)
##   gun turret       OrbGun inside the crown ring on top
##   laser emitter    the eye's lens — only open to damage while the laser charges or fires
## Positions are on the sphere's surface by (yaw around the body, elevation above the equator).
class_name OrbArmory
extends RefCounted

const POWER := Color(0.55, 1.0, 0.3)
const TUBE := Color(1.0, 0.45, 0.1)
const CORE_ANGLES := [62.5, -62.5, 117.5, -117.5] ## degrees around the body (0 = front), between legs
const CORE_ELEVATION := 12.0
const POD_ANGLE := 72.0
const POD_ELEVATION := 40.0
const HEALTH := {"core": 8.0, "pod": 6.0, "gun": 6.0, "laser": 5.0}


static func build(warden: OrbWarden, body: Node3D) -> void:
	var parts := warden.parts
	parts.clear()
	var dark := OrbEye._material(Color(0.07, 0.07, 0.08), 0.45, 0.8)
	for i in CORE_ANGLES.size():
		var node := _on_surface(body, CORE_ANGLES[i], CORE_ELEVATION, 0.97)
		var housing := CylinderMesh.new()
		housing.top_radius = 0.26
		housing.bottom_radius = 0.32
		housing.height = 0.3
		node.add_child(OrbEye._mesh(housing, Transform3D.IDENTITY, dark))
		var glow := _glow_material(POWER, 3.5)
		var cell := SphereMesh.new()
		cell.radius = 0.2
		cell.height = 0.4
		node.add_child(OrbEye._mesh(cell, Transform3D(Basis.IDENTITY, Vector3(0, 0.16, 0)), glow))
		var cage := TorusMesh.new() # a dark ring holding the cell in
		cage.inner_radius = 0.17
		cage.outer_radius = 0.23
		node.add_child(OrbEye._mesh(cage, Transform3D(Basis.IDENTITY, Vector3(0, 0.2, 0)), dark))
		var glows: Array[StandardMaterial3D] = [glow]
		parts.add("core %d" % (i + 1), "core", node, Vector3(0, 0.15, 0), 0.42, HEALTH.core, glows, POWER, 3.5)
	for side: float in [-1.0, 1.0]:
		var node := _on_surface(body, POD_ANGLE * side, POD_ELEVATION, 0.95)
		var box := BoxMesh.new()
		box.size = Vector3(0.62, 0.42, 0.8)
		node.add_child(OrbEye._mesh(box, Transform3D(Basis.IDENTITY, Vector3(0, 0.18, 0)), dark))
		var glow := _glow_material(TUBE, 2.5)
		for tx: float in [-0.15, 0.15]:
			for tz: float in [-0.2, 0.2]:
				var tube := CylinderMesh.new()
				tube.top_radius = 0.09
				tube.bottom_radius = 0.09
				tube.height = 0.06
				node.add_child(OrbEye._mesh(tube, Transform3D(Basis.IDENTITY, Vector3(tx, 0.4, tz)), glow))
		var glows: Array[StandardMaterial3D] = [glow]
		parts.add("pod %s" % ("L" if side < 0.0 else "R"), "pod", node, Vector3(0, 0.2, 0), 0.55, HEALTH.pod, glows, TUBE, 2.5)
	var gun := OrbGun.new()
	gun.name = "Gun"
	gun.target = warden.target
	gun.sparks = warden.sparks
	gun.position = Vector3(0, OrbLayout.RADIUS * 0.95, 0)
	body.add_child(gun)
	warden.gun = gun
	parts.add("gun", "gun", gun, Vector3(0, 0.3, 0), 0.5, HEALTH.gun, gun.glows, OrbGun.SIGHT, 3.0)
	var empty: Array[StandardMaterial3D] = [] # the eye lights its own lens
	parts.add("laser emitter", "laser", warden.eye.lens(), Vector3.ZERO, 0.45, HEALTH.laser, empty, Color.WHITE, 0.0, func() -> bool: return warden.laser.busy())


## Where pod `node`'s missiles leave from (world).
static func launch_point(node: Node3D) -> Vector3:
	return node.global_transform * Vector3(0, 0.5, 0)


# A node on the body sphere's surface at (yaw, elevation) degrees, its +Y pointing out.
static func _on_surface(body: Node3D, yaw: float, elevation: float, depth: float) -> Node3D:
	var a := deg_to_rad(yaw)
	var e := deg_to_rad(elevation)
	var out := Vector3(sin(a) * cos(e), sin(e), -cos(a) * cos(e))
	var x := out.cross(Vector3.UP if absf(out.y) < 0.99 else Vector3.RIGHT).normalized()
	var node := Node3D.new()
	node.transform = Transform3D(Basis(x, out, x.cross(out)), out * OrbLayout.RADIUS * depth)
	body.add_child(node)
	return node


static func _glow_material(color: Color, energy: float) -> StandardMaterial3D:
	var material := OrbEye._material(color, 0.2, 0.0)
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = energy
	return material
