## The Orb Warden's missile volley: each live pod fires PER_POD missiles a beat apart. They shoot
## straight up out of the pods, arc over and dive onto the target's area — one at the target, the
## rest scattered around it. A red ring marks every landing spot from launch (pulsing faster as
## it gets close), so the player can read where not to be. Each lands in a blast of sparks and
## molten drips: `exploded(at, radius)` — the demo hurts the player if they're inside it.
class_name OrbMissiles
extends Node3D

signal exploded(at: Vector3, radius: float)

const PER_POD := 3
const GAP := 0.14 ## seconds between launches
const FLIGHT := 1.8 ## seconds from launch to impact
const RISE := 7.0 ## metres the path climbs above the pods before turning over
const SCATTER := 3.0 ## metres around the target the other missiles land
const BLAST := 1.8 ## blast radius (m)
const MARKER := Color(1.0, 0.12, 0.05)

var sparks: OrbSparks
var ground_mask := 1

var _missiles: Array = [] ## [node, start, control, end, age (negative = waiting), marker, marker_material]


func _ready() -> void:
	top_level = true


func busy() -> bool:
	return not _missiles.is_empty()


## Fire from each of `launchers` (world points on the pods) at `target`.
func volley(launchers: Array[Vector3], target: Vector3) -> void:
	var delay := 0.0
	var first := true
	for start in launchers:
		for i in PER_POD:
			var aim := target if first else target + Vector3(randf_range(-1, 1), 0, randf_range(-1, 1)).normalized() * randf_range(1.0, SCATTER)
			first = false
			var end := _ground(aim)
			var control := start.lerp(end, 0.25) + Vector3.UP * RISE
			_missiles.append([_missile(), start, control, end, -delay, _marker(end), null])
			_missiles[-1][6] = (_missiles[-1][5] as MeshInstance3D).material_override
			delay += GAP


func clear() -> void:
	for missile: Array in _missiles:
		(missile[0] as Node3D).queue_free()
		(missile[5] as Node3D).queue_free()
	_missiles.clear()


func _process(delta: float) -> void:
	for i in range(_missiles.size() - 1, -1, -1):
		var missile: Array = _missiles[i]
		missile[4] += delta
		var age: float = missile[4]
		var node: Node3D = missile[0]
		var marker_material: StandardMaterial3D = missile[6]
		var t := clampf(age / FLIGHT, 0.0, 1.0)
		var pulse := 0.5 + 0.5 * sin(Time.get_ticks_msec() * 0.001 * lerpf(6.0, 30.0, t))
		marker_material.albedo_color = Color(MARKER, lerpf(0.25, 0.9, t) * lerpf(0.5, 1.0, pulse))
		var grow := lerpf(1.3, 1.0, t) # closes in on the blast size
		(missile[5] as Node3D).scale = Vector3(grow, 0.15, grow)
		if age < 0.0:
			node.visible = false
			continue
		node.visible = true
		var point := _bezier(missile[1], missile[2], missile[3], t)
		var ahead := _bezier(missile[1], missile[2], missile[3], minf(t + 0.02, 1.0))
		if ahead.distance_to(point) > 0.001:
			var y := (ahead - point).normalized() # capsules are built along +Y
			var x := y.cross(Vector3.UP if absf(y.y) < 0.99 else Vector3.RIGHT).normalized()
			node.global_transform = Transform3D(Basis(x, y, x.cross(y)), point)
		if sparks != null: # exhaust
			sparks.burst(point - (ahead - point).normalized() * 0.3, point - ahead, 1, 2.0, 0.3, 0.3, 0.04, missile[3].y, Color(1.0, 0.7, 0.3))
		if t >= 1.0:
			_explode(missile[3])
			node.queue_free()
			(missile[5] as Node3D).queue_free()
			_missiles.remove_at(i)


func _explode(at: Vector3) -> void:
	if sparks != null:
		sparks.burst(at, Vector3.UP, 30, 8.0, 1.0, 0.55, 0.045, at.y, Color(1.0, 0.8, 0.45))
		sparks.embers(at, 5, 3.0, at.y)
	var flash := OmniLight3D.new()
	flash.light_color = Color(1.0, 0.55, 0.2)
	flash.light_energy = 8.0
	flash.omni_range = 5.0
	add_child(flash)
	flash.global_position = at + Vector3.UP * 0.6
	var tween := flash.create_tween()
	tween.tween_property(flash, "light_energy", 0.0, 0.3)
	tween.tween_callback(flash.queue_free)
	exploded.emit(at, BLAST)


static func _bezier(a: Vector3, b: Vector3, c: Vector3, t: float) -> Vector3:
	return a.lerp(b, t).lerp(b.lerp(c, t), t)


func _missile() -> Node3D:
	var body := CapsuleMesh.new()
	body.radius = 0.09
	body.height = 0.6
	var shell := StandardMaterial3D.new()
	shell.albedo_color = Color(0.2, 0.2, 0.22)
	shell.metallic = 0.7
	var root := Node3D.new()
	var hull := MeshInstance3D.new()
	hull.mesh = body
	hull.material_override = shell
	root.add_child(hull)
	var flame := SphereMesh.new()
	flame.radius = 0.1
	flame.height = 0.3
	var hot := StandardMaterial3D.new()
	hot.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	hot.albedo_color = Color(2.5, 1.4, 0.5)
	var tail := MeshInstance3D.new()
	tail.mesh = flame
	tail.material_override = hot
	tail.position = Vector3.DOWN * 0.35
	root.add_child(tail)
	root.visible = false
	add_child(root)
	return root


# A red ring on the ground where a missile will land (radius = BLAST).
func _marker(at: Vector3) -> MeshInstance3D:
	var ring := TorusMesh.new()
	ring.inner_radius = BLAST * 0.9
	ring.outer_radius = BLAST
	ring.rings = 40
	ring.ring_segments = 4
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = Color(MARKER, 0.0)
	var marker := MeshInstance3D.new()
	marker.mesh = ring
	marker.material_override = material
	marker.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(marker)
	marker.global_transform = Transform3D(Basis.from_scale(Vector3(1, 0.15, 1)), at + Vector3.UP * 0.05)
	return marker


func _ground(point: Vector3) -> Vector3:
	var query := PhysicsRayQueryParameters3D.create(point + Vector3.UP * 20.0, point + Vector3.DOWN * 20.0, ground_mask)
	var result := get_world_3d().direct_space_state.intersect_ray(query)
	return result.position if not result.is_empty() else Vector3(point.x, 0.0, point.z)
