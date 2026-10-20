## Turns a kit's plain matte materials into the theme shaders (mood/shaders/), and drives their
## wind every frame:
##   surface  matte, like MoodProps' materials, plus wind sway (trees), a breathing glow (magical)
##            or buzzing, dropping-out neon (cyberpunk)
##   candy    glossy hard-candy: toon highlight, clear coat, rim, inner glow (the candy kit)
##   flag     a waving cloth quad (flag())
##   puddle   a black mirror with rain ripples (puddle())
## Shader materials are cached by their look, so a kit's hundred leaves share a handful of
## materials; set_wind() updates all of them.
class_name MoodShaders
extends RefCounted

const SURFACE := preload("res://mood/shaders/surface.gdshader")
const CANDY := preload("res://mood/shaders/candy.gdshader")
const FLAG := preload("res://mood/shaders/flag.gdshader")
const PUDDLE := preload("res://mood/shaders/puddle.gdshader")

static var _cache := {}
static var _windy: Array[ShaderMaterial] = [] ## every material with a wind uniform


## Convert every MeshInstance3D under `node` (its plain, lit StandardMaterial3D overrides):
## `candy` = glossy candy shader; `sways` = bends in the wind; `pulse` / `flicker` for glowing ones.
## Unshaded (lamp flames) and transparent (light shafts) materials are left alone.
static func dress(node: Node, candy: bool, sways: bool, pulse: float, flicker: float) -> void:
	if node is MeshInstance3D:
		var instance := node as MeshInstance3D
		var source := instance.material_override as StandardMaterial3D
		if source != null and source.shading_mode != BaseMaterial3D.SHADING_MODE_UNSHADED and source.transparency == BaseMaterial3D.TRANSPARENCY_DISABLED:
			instance.material_override = _material_for(source, candy, sways, pulse, flicker)
	for child in node.get_children():
		dress(child, candy, sways, pulse, flicker)


## A waving flag `size` big, its pole edge at the node's origin, facing ±Z.
static func flag(parent: Node3D, at: Vector3, size: Vector2, color: Color) -> MeshInstance3D:
	var quad := QuadMesh.new()
	quad.size = size
	quad.subdivide_width = 12
	quad.subdivide_depth = 4
	var material := ShaderMaterial.new()
	material.shader = FLAG
	material.set_shader_parameter("albedo", color)
	material.set_shader_parameter("phase", randf() * TAU)
	_windy.append(material)
	var instance := MeshInstance3D.new()
	instance.mesh = quad
	instance.material_override = material
	instance.position = at + Vector3(size.x * 0.5, 0, 0)
	parent.add_child(instance)
	return instance


## A rain puddle: a thin flat disc (radius, squashed into an oval) a hair above the ground.
static func puddle(parent: Node3D, at: Vector3, radius: float, stretch: float) -> MeshInstance3D:
	var disc := CylinderMesh.new()
	disc.top_radius = radius
	disc.bottom_radius = radius
	disc.height = 0.01
	disc.radial_segments = 20
	var material := _cache.get("puddle") as ShaderMaterial
	if material == null:
		material = ShaderMaterial.new()
		material.shader = PUDDLE
		_cache["puddle"] = material
	var instance := MeshInstance3D.new()
	instance.mesh = disc
	instance.material_override = material
	instance.position = at + Vector3.UP * 0.095
	instance.scale = Vector3(1, 1, stretch)
	instance.rotation.y = randf() * TAU
	parent.add_child(instance)
	return instance


## Wind for every shader material (strength 0 … 1, direction in degrees around +Y).
static func set_wind(strength: float, direction_degrees: float) -> void:
	var direction := Vector2.from_angle(deg_to_rad(direction_degrees))
	for i in range(_windy.size() - 1, -1, -1):
		var material := _windy[i]
		if material == null:
			_windy.remove_at(i)
			continue
		material.set_shader_parameter("wind", strength)
		material.set_shader_parameter("wind_direction", direction)


static func _material_for(source: StandardMaterial3D, candy: bool, sways: bool, pulse: float, flicker: float) -> ShaderMaterial:
	var glowing := source.emission_enabled
	var key := "%s|%s|%s|%s|%s|%.2f|%.2f|%.2f" % [candy, sways, source.albedo_color.to_html(), source.emission.to_html() if glowing else "-",
		source.emission_energy_multiplier if glowing else 0.0, pulse if glowing else 0.0, flicker if glowing else 0.0, source.rim]
	if _cache.has(key):
		return _cache[key]
	var material := ShaderMaterial.new()
	material.shader = CANDY if candy and not glowing else SURFACE
	material.set_shader_parameter("albedo", source.albedo_color)
	material.set_shader_parameter("sway", 1.0 if sways else 0.0)
	if material.shader == SURFACE:
		material.set_shader_parameter("emission", source.emission if glowing else Color.BLACK)
		material.set_shader_parameter("emission_energy", source.emission_energy_multiplier if glowing else 0.0)
		material.set_shader_parameter("pulse", pulse if glowing else 0.0)
		material.set_shader_parameter("flicker", flicker if glowing else 0.0)
	_cache[key] = material
	_windy.append(material)
	return material
