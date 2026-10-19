## The particle styles the themes use, all from one GPUParticles3D box around the camera:
##   embers     orange sparks drifting up (spooky)          sparkles   pink-white twinkles (candy)
##   sand       beige grains blowing sideways (desert)      leaves     tumbling orange-red leaves (autumn)
##   rain       fast blue-white streaks (cyberpunk)         pollen     slow cream seeds (solarpunk)
##   dust       faint warm motes hanging in the air (medieval)
##   fireflies  bright cyan-green dots wandering and pulsing (magical)
##   confetti   tumbling primary-coloured flecks (toy box)   feathers   slow white wisps (sky islands)
##   jungle     green-gold motes and insects (jungle)         glitter    golden twinkles (cloud kingdom)
##   ash        grey flakes falling, a few green spores (toxic)
## configure() rewrites the process material and the drawn quad for a style.
class_name MoodParticles
extends RefCounted

## style: [gravity, velocity (min, max), direction, spread°, turbulence, quad size, colours (ramp), additive, lifetime]
const STYLES := {
	"embers": [Vector3(0, 0.15, 0), Vector2(0.2, 0.6), Vector3.UP, 30.0, 0.6, Vector2(0.05, 0.05), [Color(1, 0.6, 0.2, 0), Color(1, 0.6, 0.2, 1), Color(1, 0.3, 0.1, 0)], true, 5.0],
	"sparkles": [Vector3(0, 0.05, 0), Vector2(0.05, 0.2), Vector3.UP, 180.0, 0.3, Vector2(0.06, 0.06), [Color(1, 0.7, 0.9, 0), Color(1, 1, 1, 1), Color(1, 0.6, 0.85, 0), Color(1, 1, 1, 1), Color(1, 0.7, 0.9, 0)], true, 4.0],
	"sand": [Vector3(0, -0.3, 0), Vector2(3.0, 5.0), Vector3(1, 0.1, 0.2), 10.0, 0.5, Vector2(0.03, 0.03), [Color(0.95, 0.82, 0.6, 0), Color(0.95, 0.82, 0.6, 0.8), Color(0.95, 0.82, 0.6, 0)], false, 3.0],
	"leaves": [Vector3(0, -0.8, 0), Vector2(0.2, 0.8), Vector3(1, -0.2, 0.3), 40.0, 1.2, Vector2(0.12, 0.09), [Color(0.95, 0.45, 0.1, 0), Color(0.95, 0.45, 0.1, 1), Color(0.85, 0.2, 0.08, 1), Color(0.95, 0.7, 0.15, 0)], false, 6.0],
	"rain": [Vector3(0, -22.0, 0), Vector2(6.0, 9.0), Vector3.DOWN, 3.0, 0.0, Vector2(0.015, 0.45), [Color(0.6, 0.8, 1.0, 0), Color(0.6, 0.8, 1.0, 0.6), Color(0.6, 0.8, 1.0, 0)], true, 1.0],
	"pollen": [Vector3(0, 0.02, 0), Vector2(0.05, 0.25), Vector3(1, 0.3, 0), 60.0, 0.8, Vector2(0.04, 0.04), [Color(1, 1, 0.9, 0), Color(1, 1, 0.9, 0.9), Color(1, 1, 0.9, 0)], true, 7.0],
	"dust": [Vector3(0, 0.01, 0), Vector2(0.02, 0.1), Vector3.UP, 180.0, 0.4, Vector2(0.025, 0.025), [Color(1, 0.9, 0.7, 0), Color(1, 0.9, 0.7, 0.5), Color(1, 0.9, 0.7, 0)], true, 8.0],
	"confetti": [Vector3(0, -0.6, 0), Vector2(0.1, 0.5), Vector3.DOWN, 60.0, 1.0, Vector2(0.07, 0.05), [Color(1, 0.2, 0.2, 0), Color(1, 0.85, 0.1, 1), Color(0.2, 0.5, 1, 1), Color(0.2, 0.8, 0.3, 1), Color(1, 0.3, 0.3, 0)], false, 6.0],
	"feathers": [Vector3(0, -0.1, 0), Vector2(0.2, 0.6), Vector3(1, 0.1, 0.3), 40.0, 0.9, Vector2(0.09, 0.05), [Color(1, 1, 1, 0), Color(1, 1, 1, 0.8), Color(1, 1, 1, 0)], false, 7.0],
	"jungle": [Vector3(0, 0.02, 0), Vector2(0.1, 0.4), Vector3.UP, 180.0, 1.3, Vector2(0.04, 0.04), [Color(0.7, 1, 0.3, 0), Color(0.9, 1, 0.4, 1), Color(0.6, 0.9, 0.2, 0)], true, 5.0],
	"glitter": [Vector3(0, -0.05, 0), Vector2(0.05, 0.2), Vector3.DOWN, 180.0, 0.4, Vector2(0.05, 0.05), [Color(1, 0.85, 0.4, 0), Color(1, 0.95, 0.6, 1), Color(1, 0.8, 0.3, 0.1), Color(1, 0.95, 0.6, 1), Color(1, 0.85, 0.4, 0)], true, 5.0],
	"ash": [Vector3(0, -0.4, 0), Vector2(0.1, 0.4), Vector3(0.5, -1, 0), 40.0, 0.8, Vector2(0.05, 0.05), [Color(0.6, 0.6, 0.55, 0), Color(0.55, 0.55, 0.5, 0.8), Color(0.7, 1.0, 0.3, 0.6), Color(0.5, 0.5, 0.45, 0)], false, 7.0],
	"fireflies": [Vector3.ZERO, Vector2(0.1, 0.4), Vector3.UP, 180.0, 1.5, Vector2(0.07, 0.07), [Color(0.4, 1, 0.8, 0), Color(0.5, 1, 0.9, 1), Color(0.3, 0.9, 1, 0.1), Color(0.5, 1, 0.8, 1), Color(0.4, 1, 0.8, 0)], true, 6.0],
}


static func configure(particles: GPUParticles3D, style: String) -> void:
	var s: Array = STYLES.get(style, STYLES.embers)
	var process := particles.process_material as ParticleProcessMaterial
	if process == null:
		process = ParticleProcessMaterial.new()
		process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
		process.emission_box_extents = Vector3(12, 5, 12)
		particles.process_material = process
	process.gravity = s[0]
	process.initial_velocity_min = s[1].x
	process.initial_velocity_max = s[1].y
	process.direction = s[2]
	process.spread = s[3]
	process.turbulence_enabled = s[4] > 0.0
	process.turbulence_noise_strength = s[4]
	var tumbles := style in ["leaves", "confetti", "feathers", "ash"]
	process.angular_velocity_min = -180.0 if tumbles else 0.0
	process.angular_velocity_max = 180.0 if tumbles else 0.0
	process.scale_min = 0.6
	process.scale_max = 1.3
	var ramp := Gradient.new()
	var colors := PackedColorArray(s[6])
	var offsets := PackedFloat32Array()
	for i in colors.size():
		offsets.append(float(i) / (colors.size() - 1))
	ramp.offsets = offsets
	ramp.colors = colors
	var ramp_texture := GradientTexture1D.new()
	ramp_texture.gradient = ramp
	process.color_ramp = ramp_texture
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD if s[7] else BaseMaterial3D.BLEND_MODE_MIX
	material.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	material.vertex_color_use_as_albedo = true
	material.albedo_color = Color(3.0, 3.0, 3.0) if s[7] else Color.WHITE # additive ones over-bright: they bloom
	var quad := QuadMesh.new()
	quad.size = s[5]
	quad.material = material
	particles.draw_pass_1 = quad
	particles.lifetime = s[8]
	particles.amount = 400 if style in ["rain", "sand"] else 260
	particles.restart()
