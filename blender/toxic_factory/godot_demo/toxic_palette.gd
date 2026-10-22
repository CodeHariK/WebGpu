## Toxic Factory materials for the meshes Godot builds itself: flat colours for the ground, yard,
## road and pipes, and the toxic river shader (toxic_river.gdshader) for the water and the sludge.
extends RefCounted

const Layout := preload("res://toxic_layout.gd")

const COLORS := {
	"dirt": Color(0.6, 0.55, 0.36),  # dead grass
	"mud": Color(0.42, 0.34, 0.24),
	"riverbed": Color(0.22, 0.26, 0.2),
	"concrete": Color(0.66, 0.65, 0.62),
	"asphalt": Color(0.24, 0.24, 0.27),
	"marking": Color(0.95, 0.9, 0.6),
	"pipe": Color(0.96, 0.62, 0.24),
	"pipe_grey": Color(0.62, 0.65, 0.7),
	"puff": Color(1, 1, 1),
}

## Water surfaces → shader settings. "river" is the polluted river (slicks spread from the
## outfalls), "pour" is pure sludge falling from a pipe, "pond" the settling pond in the yard.
const SURFACES := {
	"river": {"flow_speed": 0.9},
	"pour": {"flow_speed": 3.0, "sludge_everywhere": true, "wobble_height": 0.0, "bubble_density": 0.0},
	"pond": {"flow_speed": 0.15, "sludge_everywhere": true, "wobble_height": 0.04, "bank_width": 0.0},
}

static var _cache := {}


static func material(name: String) -> Material:
	if _cache.has(name):
		return _cache[name]
	var mat: Material
	if SURFACES.has(name):
		var water := ShaderMaterial.new()
		water.shader = preload("res://toxic_river.gdshader")
		for param: String in SURFACES[name]:
			water.set_shader_parameter(param, SURFACES[name][param])
		mat = water
	elif name == "puff":
		var smoke := StandardMaterial3D.new()
		smoke.vertex_color_use_as_albedo = true  # particles tint the puffs through COLOR
		smoke.roughness = 1.0
		mat = smoke
	else:
		var flat := StandardMaterial3D.new()
		flat.albedo_color = COLORS[name]
		flat.roughness = 0.4 if name.begins_with("pipe") else 0.9
		flat.cull_mode = BaseMaterial3D.CULL_DISABLED
		mat = flat
	_cache[name] = mat
	return mat


## Tell the river where the outfalls are, so the slicks spread downstream from them.
static func set_outfalls(points: Array[Vector2]) -> void:
	var river: ShaderMaterial = material("river")
	var packed := PackedVector2Array(points)
	while packed.size() < 4:
		packed.append(Vector2(9999, 9999))
	river.set_shader_parameter("outfalls", packed)
