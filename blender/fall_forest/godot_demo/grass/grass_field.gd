class_name GrassField
extends Node3D
## Grass over the whole terrain, drawn only round the camera, in two rings:
##   near   blades (GrassMeshes.blade: 5 triangles), dense, to near_end metres: wind, push, colour
##   far    tufts (GrassMeshes.tuft: 3 triangles, vertex-coloured), sparse, near_end .. far_end
##   past   no grass at all: the ground's own colour (Terrain) is the grass from there
## Both are opaque (no alpha), cast no shadows and fade by shrinking. The terrain is handed to
## their shaders as two textures (GrassMaps); a pusher (a ball standing in for the player, kept
## in front of the camera) shows the near blades bending away.

@export var near_density := 28.0 ## blades a square metre
@export var near_end := 26.0
@export var near_tile := 8.0
@export var far_density := 3.0 ## tufts a square metre
@export var far_end := 80.0
@export var far_tile := 16.0
@export var pusher_ahead := 5.0 ## metres in front of the camera the pusher rolls

var near: GrassLayer
var far: GrassLayer
var near_material := ShaderMaterial.new()
var far_material := ShaderMaterial.new()
var pusher := MeshInstance3D.new()


func _ready() -> void:
	var heights := GrassMaps.heights()
	var height_map := GrassMaps.height_texture(heights)
	var mask_map := GrassMaps.mask_texture(heights)
	var wind := _wind_noise()
	near_material.shader = load("res://grass/grass_near.gdshader")
	far_material.shader = load("res://grass/grass_far.gdshader")
	for material in [near_material, far_material]:
		material.set_shader_parameter(&"height_map", height_map)
		material.set_shader_parameter(&"mask_map", mask_map)
		material.set_shader_parameter(&"wind_noise", wind)
		material.set_shader_parameter(&"terrain_size", TerrainHeight.SIZE)
	near_material.set_shader_parameter(&"near_end", near_end)
	far_material.set_shader_parameter(&"near_start", near_end)
	far_material.set_shader_parameter(&"far_end", far_end)
	near = _layer("Near", near_tile, near_end, 0.0, GrassMeshes.blade(), near_density, near_material, 1)
	far = _layer("Far", far_tile, far_end, near_end - 5.0, GrassMeshes.tuft(), far_density, far_material, 2)
	_make_pusher()


func _process(_delta: float) -> void:
	var camera := get_viewport().get_camera_3d()
	if camera == null:
		return
	var ahead := -camera.global_basis.z
	ahead.y = 0.0
	var at := camera.global_position + ahead.normalized() * pusher_ahead
	at.y = TerrainHeight.height(at.x, at.z) + 0.35
	pusher.global_position = at
	near_material.set_shader_parameter(&"pusher", Vector4(at.x, at.y - 0.35, at.z, 1.3))


## Blades a square metre in the near ring, rebuilt at once (the tiles share one pattern).
func set_near_density(p_density: float) -> void:
	near_density = p_density
	near.set_count(int(p_density * near_tile * near_tile), 1)


## Tufts a square metre in the far ring.
func set_far_density(p_density: float) -> void:
	far_density = p_density
	far.set_count(int(p_density * far_tile * far_tile), 2)


## A shader uniform both rings have (wind_strength, wind_speed, color_variation, dryness).
func set_shared(p_name: StringName, p_value: float) -> void:
	near_material.set_shader_parameter(p_name, p_value)
	far_material.set_shader_parameter(p_name, p_value)


## How many blades or tufts the drawn tiles hold (before density and distance fold some away).
func instance_count(p_layer: GrassLayer) -> int:
	if p_layer.tiles.is_empty() or not p_layer.visible:
		return 0
	var shown := 0
	for tile in p_layer.tiles:
		shown += 1 if tile.visible else 0
	return shown * p_layer.multimesh.instance_count


func _layer(p_name: String, p_tile: float, p_radius: float, p_inner: float, p_mesh: Mesh, p_density: float,
		p_material: Material, p_seed: int) -> GrassLayer:
	var layer := GrassLayer.new()
	layer.name = p_name
	layer.tile_size = p_tile
	layer.radius = p_radius
	layer.inner_radius = p_inner
	add_child(layer)
	layer.build(p_mesh, int(p_density * p_tile * p_tile), p_material, p_seed)
	return layer


## Gusts: a soft noise the shaders slide across the world.
func _wind_noise() -> NoiseTexture2D:
	var noise := FastNoiseLite.new()
	noise.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	noise.frequency = 0.02
	var texture := NoiseTexture2D.new()
	texture.width = 128
	texture.height = 128
	texture.seamless = true
	texture.noise = noise
	return texture


func _make_pusher() -> void:
	var ball := SphereMesh.new()
	ball.radius = 0.35
	ball.height = 0.7
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.85, 0.25, 0.2)
	ball.material = material
	pusher.mesh = ball
	pusher.name = "Pusher"
	add_child(pusher)
