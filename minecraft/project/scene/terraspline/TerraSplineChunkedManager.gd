@tool
extends TerrainManager
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
		if val:
			rebuild_terrain()

# Container for chunk meshes
var chunks_container: Node3D = null

func _ready() -> void:
	# Connect to child splines
	for child in get_children():
		if child is TerrainSpline2D:
			if not child.is_connected("spline_changed", queue_rebuild):
				child.connect("spline_changed", queue_rebuild)
	queue_rebuild()

func queue_rebuild() -> void:
	if is_inside_tree():
		rebuild_terrain.call_deferred()

func rebuild_terrain() -> void:
	if not is_inside_tree():
		return
		
	# Ensure the container exists and is clean
	if not chunks_container:
		chunks_container = get_node_or_null("ChunksContainer")
		if not chunks_container:
			chunks_container = Node3D.new()
			chunks_container.name = "ChunksContainer"
			add_child(chunks_container)
	
	# Clear previous visual/physics chunk nodes
	for child in chunks_container.get_children():
		child.queue_free()

	clear_chunks()

	var splines = []
	for child in get_children():
		if child is TerrainSpline2D:
			splines.append(child)

	var c_size = get_chunk_size()

	# Generate chunk grid
	for z in range(-grid_radius, grid_radius + 1):
		for x in range(-grid_radius, grid_radius + 1):
			var coords = Vector2i(x, z)
			var chunk_heightmap = create_chunk(coords)
			if not chunk_heightmap:
				continue
				
			# Deform chunk with each spline, offsetting by the chunk's global coordinates
			var offset = Vector2(x * c_size, z * c_size)
			for spline in splines:
				spline.deform_heightmap(chunk_heightmap, offset)

			# Determine LOD based on distance from center
			var dist = max(abs(x), abs(z))
			var lod = 0
			if dist == 1:
				lod = 1
			elif dist >= 2:
				lod = 2

			# Generate Mesh
			var mesh = chunk_heightmap.generate_mesh(lod)
			var mesh_instance = MeshInstance3D.new()
			mesh_instance.mesh = mesh
			mesh_instance.name = "Chunk_%d_%d_LOD%d" % [x, z, lod]
			mesh_instance.position = Vector3(x * c_size, 0, z * c_size)
			if terrain_material:
				mesh_instance.material_override = terrain_material
			chunks_container.add_child(mesh_instance)

			# Generate Collision if enabled
			if enable_collision:
				var shape = chunk_heightmap.generate_collision_shape()
				if shape:
					var static_body = StaticBody3D.new()
					static_body.name = "StaticBody_%d_%d" % [x, z]
					
					var col_shape = CollisionShape3D.new()
					col_shape.name = "CollisionShape"
					col_shape.shape = shape
					
					static_body.add_child(col_shape)
					static_body.position = Vector3(x * c_size, 0, z * c_size)
					chunks_container.add_child(static_body)
