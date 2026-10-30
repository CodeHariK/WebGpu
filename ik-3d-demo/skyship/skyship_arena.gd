## The closed arena: a round floor of RADIUS metres ringed by a wall you can't leave through (the
## demo keeps the player inside: keep_inside()), with a faint grid of rings on the floor so you
## can read distances at a glance, and glowing lamps along
## the wall's top. Static helpers clamp points inside and pick random spots.
class_name SkyshipArena
extends RefCounted

const RADIUS := 50.0
const WALL_HEIGHT := 4.5
const SEGMENTS := 96


static func build(parent: Node3D) -> void:
	var floor_material := ShipKit.metal(Color(0.2, 0.2, 0.22), 0.8)
	floor_material.metallic = 0.3
	ShipKit.add(parent, ShipKit.cylinder(RADIUS + 0.6, RADIUS + 0.6, 0.4, 72), floor_material, Vector3.DOWN * 0.2)
	var line_material := ShipKit.ghost(Color(0.6, 0.65, 0.75, 0.18))
	for r: float in [18.0, 32.0, RADIUS - 0.4]:
		var ring := ShipKit.add(parent, ShipKit.ring(r - 0.04, r + 0.04), line_material, Vector3.UP * 0.01)
		ring.scale = Vector3(1, 0.05, 1)
		ring.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var wall_material := ShipKit.metal(Color(0.26, 0.24, 0.24), 0.6)
	var lamp_material := ShipKit.glow(Color(1.0, 0.45, 0.15), 2.0)
	var width := TAU * (RADIUS + 0.9) / SEGMENTS
	for i in SEGMENTS:
		var angle := TAU * i / SEGMENTS
		var out := Vector3(sin(angle), 0, cos(angle))
		var wall := ShipKit.add(parent, ShipKit.box(Vector3(width * 0.96, WALL_HEIGHT, 0.6)), wall_material,
				out * (RADIUS + 0.9) + Vector3.UP * WALL_HEIGHT * 0.5)
		wall.rotation.y = angle
		var lamp := ShipKit.add(parent, ShipKit.box(Vector3(width * 0.5, 0.12, 0.12)), lamp_material,
				out * (RADIUS + 0.55) + Vector3.UP * (WALL_HEIGHT - 0.25))
		lamp.rotation.y = angle
		if i % 3 == 0: # buttresses
			var post := ShipKit.add(parent, ShipKit.box(Vector3(0.7, WALL_HEIGHT + 1.2, 1.2)), wall_material,
					out * (RADIUS + 1.1) + Vector3.UP * (WALL_HEIGHT + 1.2) * 0.5)
			post.rotation.y = angle


## `point` moved back inside the arena, `margin` m from the wall.
static func inside(point: Vector3, margin := 0.5) -> Vector3:
	var flat := Vector3(point.x, 0, point.z)
	var limit := RADIUS - margin
	if flat.length() > limit:
		flat = flat.normalized() * limit
	return Vector3(flat.x, point.y, flat.z)


## A random floor point within `radius` of the middle (evenly spread over the disc).
static func random_point(radius: float) -> Vector3:
	var angle := randf() * TAU
	var r := sqrt(randf()) * radius
	return Vector3(sin(angle) * r, 0, cos(angle) * r)
