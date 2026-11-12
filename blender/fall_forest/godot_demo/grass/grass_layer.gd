class_name GrassLayer
extends Node3D
## One ring of grass round the camera, in square tiles that follow it.
##   one pattern    a single MultiMesh holds the roots of one tile (jittered on a grid, all at
##                  y 0) and every tile draws it: one tile's worth of memory for the whole
##                  ring, and one draw call a tile
##   following      when the camera crosses into another tile, the tiles are moved to the new
##                  ring (positions only; nothing rebuilt): no per-frame CPU work
##   culling        each tile gets a bounding box from the ground under it (custom_aabb), so
##                  the renderer skips tiles outside the view and behind hills
##   the GPU does the rest: height, density, size, wind, colour (the layer's shader)

var tile_size := 8.0
var radius := 30.0 ## metres: tiles reaching past this aren't made
var inner_radius := 0.0 ## metres: tiles entirely inside this aren't made (another layer is there)
var grass_height := 1.0 ## metres of headroom in a tile's bounding box

var tiles: Array[MultiMeshInstance3D] = []
var multimesh: MultiMesh # the one pattern every tile draws
var _slots: Array[Vector2i] = [] # each tile's place, in tiles from the camera's tile
var _camera_tile := Vector2i(1 << 30, 1 << 30)


## p_mesh drawn p_per_tile times a tile, with p_material.
func build(p_mesh: Mesh, p_per_tile: int, p_material: Material, p_seed: int) -> void:
	multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.mesh = p_mesh
	set_count(p_per_tile, p_seed)
	var reach := int(ceil(radius / tile_size)) + 1
	for z in range(-reach, reach + 1):
		for x in range(-reach, reach + 1):
			var nearest := _nearest_distance(x, z)
			var farthest := _farthest_distance(x, z)
			if nearest > radius or farthest < inner_radius:
				continue
			var tile := MultiMeshInstance3D.new()
			tile.multimesh = multimesh
			tile.material_override = p_material
			tile.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF # grass casts none
			add_child(tile)
			tiles.append(tile)
			_slots.append(Vector2i(x, z))


func _process(_delta: float) -> void:
	var camera := get_viewport().get_camera_3d()
	if camera == null:
		return
	var at := Vector2i(floori(camera.global_position.x / tile_size), floori(camera.global_position.z / tile_size))
	if at == _camera_tile:
		return
	_camera_tile = at
	var half := TerrainHeight.SIZE * 0.5
	for i in tiles.size():
		var corner := Vector2(at + _slots[i]) * tile_size
		var inside := corner.x >= -half and corner.y >= -half and corner.x + tile_size <= half and corner.y + tile_size <= half
		tiles[i].visible = inside
		if inside:
			tiles[i].position = Vector3(corner.x, 0.0, corner.y)
			tiles[i].custom_aabb = _ground_box(corner)


## About p_count roots a tile: a grid of cells, one root at a random spot in each (no clumps,
## no gaps). Every tile draws the new pattern at once (they share it).
func set_count(p_count: int, p_seed: int = 1) -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = p_seed
	var cells := int(ceil(sqrt(float(maxi(p_count, 0)))))
	var cell := tile_size / maxi(cells, 1)
	multimesh.instance_count = cells * cells
	for i in cells * cells:
		var spot := Vector3(((i % cells) + rng.randf()) * cell, 0.0, ((i / cells) + rng.randf()) * cell)
		multimesh.set_instance_transform(i, Transform3D(Basis(), spot))


## The tile's box from the ground's lowest to highest point (9 heights) plus the grass.
func _ground_box(p_corner: Vector2) -> AABB:
	var low := INF
	var high := -INF
	for j in 3:
		for i in 3:
			var h := TerrainHeight.height(p_corner.x + i * tile_size * 0.5, p_corner.y + j * tile_size * 0.5)
			low = minf(low, h)
			high = maxf(high, h)
	return AABB(Vector3(-1.0, low - 0.5, -1.0), Vector3(tile_size + 2.0, high - low + grass_height + 1.0, tile_size + 2.0))


## Nearest and farthest a point in the tile (x, z) away can be from a camera anywhere in tile 0.
func _nearest_distance(p_x: int, p_z: int) -> float:
	return Vector2(maxi(absi(p_x) - 1, 0), maxi(absi(p_z) - 1, 0)).length() * tile_size


func _farthest_distance(p_x: int, p_z: int) -> float:
	return Vector2(absi(p_x) + 1, absi(p_z) + 1).length() * tile_size
