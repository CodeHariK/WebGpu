@tool
extends TerrainManagerOld
class_name TerraSplineChunkedManager

@export_group("Grid Configuration")
@export var grid_radius: int = 2:
	set(val):
		grid_radius = val
		queue_rebuild()

@export var default_elevation: float = 0.0:
	set(val):
		default_elevation = val
		queue_rebuild()

@export var terrain_material: Material:
	set(val):
		terrain_material = val
		queue_rebuild()

@export var enable_collision: bool = true:
	set(val):
		enable_collision = val
		queue_rebuild()

@export var rebuild_now: bool = false:
	set(val):
		# 1. BUTTON HACK: Instantly set this back to false so the Inspector 
		# checkbox unchecks itself automatically, acting like a true button.
		rebuild_now = false
		if val:
			# 2. SCENE LOCK FIX: Use queue_rebuild to defer the tree modification
			# until after the Inspector is done updating.
			queue_rebuild()

var chunk_cache: Dictionary = {}
var chunks_container: Node3D = null

# 3. DEBOUNCE FLAG: Prevents 60+ rebuilds from queueing up in a single frame
var _rebuild_queued: bool = false

func _ready() -> void:
	# if not Engine.is_editor_hint():
	# 	return
	for child in get_children():
		_connect_spline(child)
		
	queue_rebuild()

func _on_child_entered_tree(node: Node) -> void:
	_connect_spline(node)

func _connect_spline(node: Node) -> void:
	if node is TerrainSpline2DOld:
		if not node.is_connected("spline_changed", queue_rebuild):
			node.connect("spline_changed", queue_rebuild)

func queue_rebuild() -> void:
	# Only queue a rebuild if one isn't already waiting for the next frame
	if is_inside_tree() and not _rebuild_queued:
		_rebuild_queued = true
		call_deferred("_execute_rebuild")

func _execute_rebuild() -> void:
	# Reset the flag and execute the heavy math
	_rebuild_queued = false
	rebuild_terrain()

func rebuild_terrain() -> void:
	if not is_inside_tree():
		return
		
	if not is_instance_valid(chunks_container):
		chunks_container = get_node_or_null("ChunksContainer")
		if not is_instance_valid(chunks_container):
			chunks_container = Node3D.new()
			chunks_container.name = "ChunksContainer"
			add_child(chunks_container)
			
			if Engine.is_editor_hint() and get_tree().edited_scene_root:
				chunks_container.owner = get_tree().edited_scene_root
			
			chunk_cache.clear()

	clear_chunks()

	var splines = []
	for child in get_children():
		if child is TerrainSpline2DOld:
			splines.append(child)

	var c_size = get_chunk_size()
	var active_coords = []

	for z in range(-grid_radius, grid_radius + 1):
		for x in range(-grid_radius, grid_radius + 1):
			var coords = Vector2i(x, z)
			active_coords.append(coords)
			
			var chunk_heightmap = create_chunk(coords)
			if not chunk_heightmap:
				continue
				
			var offset = Vector2(x * c_size, z * c_size)
			for spline in splines:
				spline.deform_heightmap(chunk_heightmap, offset)

			var dist = max(abs(x), abs(z))
			var lod = 0
			if dist == 1:
				lod = 1
			elif dist >= 2:
				lod = 2

			var new_mesh = chunk_heightmap.generate_mesh(lod)
			var new_shape = null
			if enable_collision:
				new_shape = chunk_heightmap.generate_collision_shape()

			if chunk_cache.has(coords) and not is_instance_valid(chunk_cache[coords]["mesh_instance"]):
				chunk_cache.erase(coords)

			if not chunk_cache.has(coords):
				_spawn_chunk_nodes(coords, x, z, c_size)
				
			var cache_entry = chunk_cache[coords]
			
			cache_entry["mesh_instance"].mesh = new_mesh
			if terrain_material:
				cache_entry["mesh_instance"].material_override = terrain_material
				
			if enable_collision and new_shape:
				cache_entry["collision_shape"].shape = new_shape
			elif cache_entry["collision_shape"]:
				cache_entry["collision_shape"].shape = null

	var coords_to_delete = []
	for cached_coord in chunk_cache.keys():
		if not active_coords.has(cached_coord):
			if is_instance_valid(chunk_cache[cached_coord]["mesh_instance"]):
				chunk_cache[cached_coord]["mesh_instance"].queue_free()
			if is_instance_valid(chunk_cache[cached_coord]["static_body"]):
				chunk_cache[cached_coord]["static_body"].queue_free()
			coords_to_delete.append(cached_coord)
			
	for c in coords_to_delete:
		chunk_cache.erase(c)

func _spawn_chunk_nodes(coords: Vector2i, x: int, z: int, c_size: int) -> void:
	var mi = MeshInstance3D.new()
	mi.name = "Chunk_%d_%d" % [x, z]
	mi.position = Vector3(x * c_size, 0, z * c_size)
	chunks_container.add_child(mi)
	
	var sb = StaticBody3D.new()
	sb.name = "StaticBody_%d_%d" % [x, z]
	sb.position = Vector3(x * c_size, 0, z * c_size)
	
	var cs = CollisionShape3D.new()
	cs.name = "CollisionShape"
	sb.add_child(cs)
	chunks_container.add_child(sb)
		
	chunk_cache[coords] = {
		"mesh_instance": mi,
		"static_body": sb,
		"collision_shape": cs
	}
