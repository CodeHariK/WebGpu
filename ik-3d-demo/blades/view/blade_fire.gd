## The blades' fire: a FireTrail per blade (fire_trail.gdshader, built up in 4 steps — next_step()
## cycles them), embers shed by fast blades, and reusable spark bursts for hits. Also turns on
## glow, so colours above 1.0 bloom.
class_name BladeFire
extends Node3D

const TRAIL_SPAN := 0.9 ## the trail spans this much of the blade, back from its tip
const EMBER_SPEED := 6.0 ## blades faster than this (m/s) shed embers
const STEP_NAMES := ["fade", "colour by age", "flicker", "cartoon"]

var _material: ShaderMaterial
var _trails: Array[FireTrail] = [] ## one per blade
var _embers: Array[CPUParticles3D] = [] ## one per blade
var _sparks: Array[CPUParticles3D] = [] ## hit bursts, reused round robin
var _next_spark := 0


func setup(environment: Environment) -> void:
	var noise := FastNoiseLite.new()
	noise.frequency = 0.035
	noise.fractal_octaves = 3
	var texture := NoiseTexture2D.new()
	texture.width = 256
	texture.height = 64
	texture.seamless = true
	texture.noise = noise
	_material = ShaderMaterial.new()
	_material.shader = load("res://blades/fire_trail.gdshader")
	_material.set_shader_parameter("noise", texture)
	_material.set_shader_parameter("fire_step", 4)
	for i in 2:
		var trail := FireTrail.new(_material)
		add_child(trail)
		_trails.append(trail)
		var embers := _particles(14, 0.45, 1.5, 0.06)
		embers.one_shot = false
		embers.explosiveness = 0.0
		embers.emitting = false
		_embers.append(embers)
	for i in 4:
		var burst := _particles(26, 0.4, 7.0, 0.08)
		burst.one_shot = true
		burst.explosiveness = 1.0
		burst.emitting = false
		_sparks.append(burst)
	environment.glow_enabled = true
	environment.glow_hdr_threshold = 1.0
	environment.glow_intensity = 0.5


func clear() -> void:
	for trail in _trails:
		trail.clear_trail()


## Each moving blade adds a sample (its base and tip) to its trail; fast blades shed embers.
func draw(
		blades: Array[Blade],
		delta: float
) -> void:
	for b in blades.size():
		var blade := blades[b]
		var moving := blade.phase != Blade.IDLE and blade.phase != Blade.HOLD and blade.delay <= 0.0
		if moving:
			var heading := -blade.mesh.global_basis.z # the blade points along its −Z
			_trails[b].add_sample(blade.tip - heading * TRAIL_SPAN, blade.tip)
		_trails[b].rebuild()
		var speed := blade.tip.distance_to(blade.previous_tip) / maxf(delta, 1e-4)
		_embers[b].global_position = blade.tip
		_embers[b].emitting = moving and speed > EMBER_SPEED


func spark(at: Vector3) -> void:
	var burst := _sparks[_next_spark]
	_next_spark = (_next_spark + 1) % _sparks.size()
	burst.global_position = at
	burst.restart()


func step() -> int:
	return int(_material.get_shader_parameter("fire_step"))


func step_name() -> String:
	return "%d %s" % [step(), STEP_NAMES[step() - 1]]


func next_step() -> void:
	_material.set_shader_parameter("fire_step", step() % 4 + 1)


# Little glowing squares that fly out, fall and shrink: sparks and embers.
func _particles(
		amount: int,
		lifetime: float,
		speed: float,
		size: float
) -> CPUParticles3D:
	var particles := CPUParticles3D.new()
	particles.amount = amount
	particles.lifetime = lifetime
	particles.local_coords = false
	particles.direction = Vector3.UP
	particles.spread = 180.0
	particles.initial_velocity_min = speed * 0.4
	particles.initial_velocity_max = speed
	particles.gravity = Vector3(0, -9.8, 0)
	particles.damping_min = 2.0
	particles.damping_max = 4.0
	var shrink := Curve.new()
	shrink.add_point(Vector2(0.0, 1.0))
	shrink.add_point(Vector2(1.0, 0.0))
	particles.scale_amount_curve = shrink
	var quad := QuadMesh.new()
	quad.size = Vector2.ONE * size
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	material.billboard_keep_scale = true # let the particles shrink
	material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	material.albedo_color = Color(2.5, 1.2, 0.3) # above 1: glows
	quad.material = material
	particles.mesh = quad
	add_child(particles)
	return particles
