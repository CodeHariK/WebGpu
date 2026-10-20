## The hall's WorldEnvironment: near-black purple void, coloured haze (depth fog), strong glow so
## the neon blooms, filmic tonemap. No SSR / SSAO / volumetric fog (mobile budget).
class_name DiscoEnv
extends RefCounted

const BACKGROUND := Color(0.012, 0.006, 0.025)
const HAZE := Color(0.16, 0.06, 0.26)
const AMBIENT := Color(0.3, 0.18, 0.5)


static func build(parent: Node) -> Environment:
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = BACKGROUND
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = AMBIENT
	environment.ambient_light_energy = 0.5
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment.tonemap_exposure = 1.0
	environment.glow_enabled = true
	environment.glow_intensity = 0.9
	environment.glow_strength = 1.1
	environment.glow_bloom = 0.08
	environment.glow_hdr_threshold = 0.9
	environment.glow_blend_mode = Environment.GLOW_BLEND_MODE_ADDITIVE
	environment.fog_enabled = true
	environment.fog_mode = Environment.FOG_MODE_EXPONENTIAL
	environment.fog_light_color = HAZE
	environment.fog_density = 0.012
	environment.fog_sky_affect = 0.0
	environment.adjustment_enabled = true
	environment.adjustment_saturation = 1.2
	environment.adjustment_contrast = 1.08
	var world := WorldEnvironment.new()
	world.environment = environment
	parent.add_child(world)
	return environment
