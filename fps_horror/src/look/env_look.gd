## Mobile materials for the mine, made from the original's (shrunk) environment textures.
## Triplanar mapping, so plain boxes and their wall meshes don't need UVs lined up: one texture
## lookup per axis, which is fine on mobile for a few big surfaces.
extends RefCounted

const TEX := "res://assets/env/textures/"

static var _cache := {}


## Rough rock for walls and ceilings.
static func rock() -> StandardMaterial3D:
	return _triplanar("rock", "Rock2_Albedo.png", "Rock2_Normals.png", "Rock2_Roughness.png", 0.35, Color(0.8, 0.78, 0.75))


## Gravel for floors.
static func gravel() -> StandardMaterial3D:
	return _triplanar("gravel", "Gravel1_Albedo.png", "Gravel1_Normals.png", "Gravel1_Roughness.png", 0.5, Color.WHITE)


## Wooden planks for crates, stairs and the mine frames.
static func planks() -> StandardMaterial3D:
	return _triplanar("planks", "RailCart/WoodPlanksAlbedo.png", "RailCart/WoodPlanksNormalMap.png",
		"RailCart/WoodPlanksRoughness.png", 0.6, Color(0.85, 0.8, 0.75))


## Rusty metal for the rail cart.
static func rust() -> StandardMaterial3D:
	var mat := _triplanar("rust", "RailCart/RustyMetalAlbedo.png", "RailCart/RustyMetalNormalMap.png",
		"RailCart/RustyMetalRoughness.png", 0.8, Color.WHITE)
	mat.metallic = 0.6
	return mat


## A bare light bulb: just glows (the light itself is an OmniLight3D next to it).
static func bulb() -> StandardMaterial3D:
	if not _cache.has("bulb"):
		var mat := StandardMaterial3D.new()
		mat.albedo_color = Color(1.0, 0.8, 0.5)
		mat.emission_enabled = true
		mat.emission = Color(1.0, 0.75, 0.4)
		mat.emission_energy_multiplier = 4.0
		_cache["bulb"] = mat
	return _cache["bulb"]


static func _triplanar(key: String, albedo: String, normal: String, rough: String, scale: float, tint: Color) -> StandardMaterial3D:
	if _cache.has(key):
		return _cache[key]
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = load(TEX + albedo)
	mat.albedo_color = tint
	mat.normal_enabled = true
	mat.normal_texture = load(TEX + normal)
	mat.roughness_texture = load(TEX + rough)
	mat.uv1_triplanar = true
	mat.uv1_world_triplanar = true
	mat.uv1_scale = Vector3.ONE * scale
	_cache[key] = mat
	return mat
