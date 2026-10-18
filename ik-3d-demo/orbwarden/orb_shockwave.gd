## Ground shockwaves from the Orb Warden's stomp: a glowing ring that races out over the ground,
## thinning and fading as it goes, with a dust-and-spark spray along its front.
## check_hit() tells whether a ring's front is passing a point right now: a grounded player there
## is hit (once per ring); jumping over the front dodges it.
class_name OrbShockwave
extends Node3D

const SPEED := 13.0 ## m/s the ring grows
const MAX_RADIUS := 15.0
const FRONT := 0.9 ## metres either side of the ring's radius that count as "in the wave"
const COLOR := Color(1.0, 0.55, 0.15)

var sparks: OrbSparks

var _rings: Array = [] ## [MeshInstance3D, StandardMaterial3D, centre, radius, hit_done]


func _ready() -> void:
	top_level = true


func spawn(center: Vector3) -> void:
	var torus := TorusMesh.new()
	torus.inner_radius = 0.93
	torus.outer_radius = 1.0
	torus.rings = 64
	torus.ring_segments = 6
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	material.albedo_color = COLOR
	var ring := MeshInstance3D.new()
	ring.mesh = torus
	ring.material_override = material
	ring.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(ring)
	_rings.append([ring, material, center, 0.3, false])


## Knock-back direction if a ring's front is crossing `point` now and the player isn't airborne
## (each ring hits only once); zero otherwise.
func check_hit(point: Vector3, airborne: bool) -> Vector3:
	for ring: Array in _rings:
		var offset: Vector3 = (point - ring[2]) * Vector3(1, 0, 1)
		if ring[4] or airborne or absf(offset.length() - ring[3]) > FRONT:
			continue
		ring[4] = true
		return offset.normalized() if offset.length() > 0.01 else Vector3.BACK
	return Vector3.ZERO


func clear() -> void:
	for ring: Array in _rings:
		(ring[0] as Node3D).queue_free()
	_rings.clear()


func _process(delta: float) -> void:
	for i in range(_rings.size() - 1, -1, -1):
		var ring: Array = _rings[i]
		ring[3] += SPEED * delta
		var radius: float = ring[3]
		var t := radius / MAX_RADIUS
		if t >= 1.0:
			(ring[0] as Node3D).queue_free()
			_rings.remove_at(i)
			continue
		var node: MeshInstance3D = ring[0]
		node.global_transform = Transform3D(Basis.from_scale(Vector3(radius, 0.6 + 2.0 * (1.0 - t), radius)), ring[2] + Vector3.UP * 0.08)
		(ring[1] as StandardMaterial3D).albedo_color = Color(COLOR, 0.9 * (1.0 - t) * (1.0 - t))
		if sparks != null and t < 0.7: # dust kicked up along the front
			for k in 3:
				var angle := randf() * TAU
				var spot: Vector3 = ring[2] + Vector3(cos(angle), 0, sin(angle)) * radius
				sparks.burst(spot, Vector3(cos(angle), 2.0, sin(angle)), 1, 4.0 * (1.0 - t), 0.4, 0.4, 0.05, ring[2].y, Color(1.0, 0.75, 0.4))
