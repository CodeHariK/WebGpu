## Choco Island colours (sRGB, same values as the Blender kit) and the materials for the meshes
## Godot builds itself: flat colours for the cake and patches, and the chocolate river shader
## (choco_river.gdshader) for the syrup surfaces, each with its own flow settings.
extends RefCounted

const COLORS := {
	"syrup": Color(0.36, 0.18, 0.09),
	"syrup_flow": Color(0.52, 0.27, 0.12),
	"syrup_light": Color(0.72, 0.45, 0.24),
	"choco": Color(0.42, 0.23, 0.13),
	"choco_dark": Color(0.28, 0.15, 0.09),
	"sponge": Color(0.62, 0.38, 0.22),
	"cream": Color(0.98, 0.94, 0.88),
	"frost_pink": Color(0.96, 0.55, 0.74),
	"frost_white": Color(0.99, 0.97, 0.97),
	"pistachio": Color(0.56, 0.78, 0.42),
	"foam": Color(0.86, 0.7, 0.55),
}

## Glossy things (syrup, frosting); everything else is 0.75 rough.
const ROUGHNESS := {
	"syrup": 0.12,
	"syrup_flow": 0.1,
	"syrup_light": 0.15,
	"frost_pink": 0.35,
	"frost_white": 0.35,
	"foam": 0.3,
}

## Syrup surfaces → river shader settings. Falls run fast; the pool and sea barely drift and
## have no banks (their UVs are world positions, not a river shape).
const SYRUP := {
	"river": {"flow_speed": 1.2},
	"fall": {"flow_speed": 3.5, "wobble_height": 0.0, "streak_length": 5.0, "bubble_density": 0.02},
	"pool": {"flow_speed": 0.3, "bank_width": 0.0, "marble_lines": 1.5, "wobble_height": 0.03},
	"sea": {"flow_speed": 0.25, "bank_width": 0.0, "marble_lines": 0.6, "wobble_height": 0.07,
		"wobble_length": 7.0, "streak_amount": 0.35, "bubble_density": 0.015, "marble_amount": 0.5},
}

static var _cache := {}
static var river_shader: Shader = preload("res://choco_river.gdshader")


## Swap the shader on every syrup surface (the demo uses this to step through river_steps/).
static func set_river_shader(shader: Shader) -> void:
	river_shader = shader
	for key: String in SYRUP:
		if _cache.has(key):
			_cache[key].shader = shader


## One shared material per name: a river-shader material for the syrup surfaces, otherwise a flat
## colour. Culling is off so ribbons and caps never vanish from behind.
static func material(color_name: String) -> Material:
	if _cache.has(color_name):
		return _cache[color_name]
	if SYRUP.has(color_name):
		var syrup := ShaderMaterial.new()
		syrup.shader = river_shader
		for param: String in SYRUP[color_name]:
			syrup.set_shader_parameter(param, SYRUP[color_name][param])
		_cache[color_name] = syrup
		return syrup
	var mat := StandardMaterial3D.new()
	mat.albedo_color = COLORS[color_name]
	mat.roughness = ROUGHNESS.get(color_name, 0.75)
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	_cache[color_name] = mat
	return mat
