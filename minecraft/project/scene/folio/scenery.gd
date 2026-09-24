extends Node3D
##
## Lightweight port of folio World/Scenery.js: instance the authored scenery.glb
## (track curbs, rocks, fences, bridge, road) and (1) re-shade every surface with
## the folio pipeline so props match the world, (2) give solid objects a trimesh
## collider so the car can drive the track and bump into things.
##
## Folio bakes most object colours into a shared PALETTE texture (UV -> swatch);
## a few big pieces (the road, some hulls) use a plain solid colour instead. So we
## pick per surface: textured -> mesh_scenery (samples the palette), solid ->
## mesh_default (its own base_color). Both run the same folio_shade/light pipeline.
##
## Names carry a `ref` prefix for reference-only nodes; we don't collide those.
##

@export var scenery_path := "res://assets/folio/scenery/scenery.glb"
@export var palette_path := "res://assets/folio/scenery/scenery_palette.png"

var _road_mat: ShaderMaterial         # folio dark road + glitter (the road mesh)
var _solid_shader: Shader             # mesh_default, instanced per unique colour
var _scenery_shader: Shader           # mesh_scenery, instanced per source texture
var _glow_shader: Shader              # mesh_glow, instanced per glow colour
var _default_palette: Texture2D       # fallback palette (this loader's palette_path)
var _solid_cache := {}                # html colour -> ShaderMaterial
var _tex_cache := {}                  # Texture2D -> ShaderMaterial (mesh_scenery)
var _glow_cache := {}                 # html colour -> ShaderMaterial (mesh_glow)

func _ready() -> void:
	var scn: PackedScene = load(scenery_path)
	if scn == null:
		push_warning("FolioScenery: could not load " + scenery_path)
		return
	_scenery_shader = load("res://material/shaders/folio/mesh_scenery.gdshader")
	_glow_shader = load("res://material/shaders/folio/mesh_glow.gdshader")
	_default_palette = load(palette_path)
	_road_mat = ShaderMaterial.new()
	_road_mat.shader = load("res://material/shaders/folio/mesh_road.gdshader")
	_road_mat.set_shader_parameter("has_water", false)
	_road_mat.set_shader_parameter("has_reveal", false)
	_solid_shader = load("res://material/shaders/folio/mesh_default.gdshader")
	var root := scn.instantiate()
	add_child(root)
	_dress(root)

func _dress(n: Node) -> void:
	for c in n.get_children():
		if c is MeshInstance3D:
			_apply_materials(c as MeshInstance3D)
			if not str(c.name).to_lower().begins_with("ref"):
				(c as MeshInstance3D).create_trimesh_collision()
		_dress(c)

func _apply_materials(mi: MeshInstance3D) -> void:
	var mesh := mi.mesh
	if mesh == null:
		return
	var is_road := str(mi.name).to_lower().find("road") >= 0
	for i in range(mesh.get_surface_count()):
		var fmat: Material = _road_mat if is_road else _folio_material_for(mi.get_active_material(i))
		if fmat:
			mi.set_surface_override_material(i, fmat)

# Route each source material to a folio material by its glb material NAME + texture:
#  - "emissive*"  -> unshaded mesh_glow (lantern/pole lights, ramp stripes, area signs)
#  - has a texture -> mesh_scenery sampling THAT texture (palette swatches OR real
#                     maps/labels/logos; both are plain UV lookups)
#  - solid colour  -> mesh_default with its own base_color
func _folio_material_for(src: Material) -> Material:
	var std := src as StandardMaterial3D
	var mname := ""
	if src:
		mname = str(src.resource_name).to_lower()
	if mname.begins_with("emissive"):
		var gcol := Color(1.0, 0.6, 0.2)
		if std:
			gcol = std.emission if std.emission_enabled else std.albedo_color
		return _glow_mat(gcol)
	if std and std.albedo_texture != null:
		return _textured_mat(std.albedo_texture)
	var col: Color = std.albedo_color if std else Color.WHITE
	var key := col.to_html(false)
	if not _solid_cache.has(key):
		var m := ShaderMaterial.new()
		m.shader = _solid_shader
		m.set_shader_parameter("base_color", col)
		m.set_shader_parameter("has_water", false)
		m.set_shader_parameter("has_reveal", false)
		_solid_cache[key] = m
	return _solid_cache[key]

# mesh_scenery bound to a specific texture (each unique source texture cached once).
func _textured_mat(tex: Texture2D) -> Material:
	if not _tex_cache.has(tex):
		var m := ShaderMaterial.new()
		m.shader = _scenery_shader
		m.set_shader_parameter("palette", tex if tex else _default_palette)
		m.set_shader_parameter("has_water", false)
		m.set_shader_parameter("has_reveal", false)
		_tex_cache[tex] = m
	return _tex_cache[tex]

# Unshaded glow (folio emissive parts): energy signs, lantern glass, ramp stripes.
func _glow_mat(col: Color) -> Material:
	var key := col.to_html(false)
	if not _glow_cache.has(key):
		var m := ShaderMaterial.new()
		m.shader = _glow_shader
		m.set_shader_parameter("glow_color", col)
		_glow_cache[key] = m
	return _glow_cache[key]
