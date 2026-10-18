## Glowing particles for the Orb Warden: sparks (small, fast, short-lived streaks) and embers /
## molten drips (bigger, slower blobs that fall, bounce once and cool from yellow to dull red).
## Pure CPU particles drawn with one MultiMesh of stretched spheres, so every spark of every effect
## is one draw call. Each particle falls with GRAVITY and bounces off its own floor height (the
## ground where it was born), losing most of its speed.
class_name OrbSparks
extends MultiMeshInstance3D

const MAX := 600
const GRAVITY := 14.0
const BOUNCE := 0.3 ## share of the downward speed kept on a bounce
const STRETCH := 0.035 ## seconds of motion each spark is stretched over (motion blur)
const HOT := Color(1.0, 0.95, 0.7)
const COOL := Color(0.9, 0.12, 0.02)

var _particles: Array = [] ## [position, velocity, age, life, size, floor_y, color]


func _ready() -> void:
	top_level = true
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var ball := SphereMesh.new()
	ball.radius = 1.0
	ball.height = 2.0
	ball.radial_segments = 6
	ball.rings = 3
	multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.mesh = ball
	multimesh.instance_count = MAX
	multimesh.visible_instance_count = 0
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	material.albedo_color = Color(2.5, 2.5, 2.5) # over-bright: glows with the environment's glow
	material_override = material


## `count` sparks from `at`, flying around `direction` (spread 0 = straight along it, 1 = any way
## in that hemisphere) at about `speed` m/s. `tint` colours the hot start (white-yellow by default).
func burst(at: Vector3, direction: Vector3, count: int, speed: float, spread := 0.7, life := 0.5, size := 0.035, floor_y := NAN, tint := HOT) -> void:
	var ground := at.y - 0.02 if is_nan(floor_y) else floor_y
	var axis := direction.normalized() if direction.length() > 0.001 else Vector3.UP
	for i in count:
		if _particles.size() >= MAX:
			_particles.pop_front()
		var random := Vector3(randf_range(-1, 1), randf_range(-1, 1), randf_range(-1, 1))
		var dir := (axis + random * spread).normalized()
		if dir.dot(axis) < 0.0:
			dir = (dir - axis * dir.dot(axis) * 2.0).normalized()
		var velocity := dir * speed * randf_range(0.5, 1.2)
		_particles.append([at, velocity, 0.0, life * randf_range(0.6, 1.3), size * randf_range(0.7, 1.4), ground, tint])


## Molten drips: a few bigger, slower blobs that arc out, fall, bounce and cool.
func embers(at: Vector3, count: int, speed := 2.5, floor_y := NAN) -> void:
	burst(at, Vector3.UP, count, speed, 0.9, 1.6, 0.07, floor_y, Color(1.0, 0.7, 0.2))


func _process(delta: float) -> void:
	var shown := 0
	var i := 0
	while i < _particles.size():
		var p: Array = _particles[i]
		p[2] += delta
		if p[2] >= p[3]:
			_particles.remove_at(i)
			continue
		var velocity: Vector3 = p[1]
		velocity.y -= GRAVITY * delta
		var point: Vector3 = p[0] + velocity * delta
		if point.y < p[5] and velocity.y < 0.0: # bounce (and skid) on the floor
			point.y = p[5]
			velocity = Vector3(velocity.x * 0.5, -velocity.y * BOUNCE, velocity.z * 0.5)
		p[0] = point
		p[1] = velocity
		var heat: float = 1.0 - p[2] / p[3]
		var color := COOL.lerp(p[6], heat * heat)
		color.a = 1.0
		var size: float = p[4] * (0.4 + 0.6 * heat)
		var basis := _stretched(velocity, size)
		multimesh.set_instance_transform(shown, Transform3D(basis, point))
		multimesh.set_instance_color(shown, color)
		shown += 1
		i += 1
	multimesh.visible_instance_count = shown


# A sphere scaled to `size`, stretched along its velocity (a streak).
static func _stretched(velocity: Vector3, size: float) -> Basis:
	var speed := velocity.length()
	if speed < 0.01:
		return Basis.from_scale(Vector3.ONE * size)
	var y := velocity / speed
	var x := y.cross(Vector3.UP if absf(y.y) < 0.99 else Vector3.RIGHT).normalized()
	var z := x.cross(y)
	var length := maxf(size, speed * STRETCH)
	return Basis(x * size, y * length, z * size)
