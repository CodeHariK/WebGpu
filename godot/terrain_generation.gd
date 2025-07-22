class_name TerrainGeneration
extends Node

@export var size_width := 100.0
@export var size_depth := 100.0
@export var mesh_resolution := 100
@export var noise: FastNoiseLite

func _ready() -> void:
	generate()

func generate():
	# Generate a base plane
	var plane_mesh = PlaneMesh.new()
	plane_mesh.size = Vector2(size_width, size_depth)
	plane_mesh.subdivide_width = mesh_resolution
	plane_mesh.subdivide_depth = mesh_resolution
	plane_mesh.material = preload("res://TerrainMaterial.tres")

	# Use MeshDataTool to modify vertices
	var mdt = MeshDataTool.new()
	var mesh = ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, plane_mesh.surface_get_arrays(0))
	mdt.create_from_surface(mesh, 0)

	for i in range(mdt.get_vertex_count()):
		var v = mdt.get_vertex(i)
		# Use noise or random
		v.y = noise.get_noise_2d(v.x, v.z) * 10.0
		mdt.set_vertex(i, v)

	mdt.commit_to_surface(mesh)
	mesh.surface_remove(0) # remove old surface
	mdt.commit_to_surface(mesh)

	# Add mesh instance
	var mi = MeshInstance3D.new()
	mi.mesh = mesh
	mi.create_trimesh_collision()
	add_child(mi)

#class_name TerrainGeneration
#extends Node
#
#var mesh: MeshInstance3D
#var size_depth := 100
#var size_width := 100
#var mesh_resolution := 100
#
#@export var noise: FastNoiseLite
#
#func _ready() -> void:
	#generate()
#
#func generate():
	#var plane_mesh = PlaneMesh.new()
	#plane_mesh.size = Vector2(size_width, size_depth)
	#plane_mesh.subdivide_depth = size_depth * mesh_resolution
	#plane_mesh.subdivide_width = size_width * mesh_resolution
	#plane_mesh.material = preload("res://TerrainMaterial.tres")
#
	#var surface = SurfaceTool.new()
	#var data = MeshDataTool.new()
	#surface.create_from(plane_mesh, 0)
#
	#var array_plane = surface.commit()
	#data.create_from_surface(array_plane, 0)
#
	#for i in range(data.get_vertex_count()):
		#var vertex = data.get_vertex(i)
		#vertex.y = randf_range(0, 10)
		#data.set_vertex(i, vertex)
#
	#array_plane.clear_surfaces()
#
	#data.commit_to_surface(array_plane)
	#surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	#surface.create_from(array_plane, 0)
	#surface.generate_normals()
#
	#mesh = MeshInstance3D.new()
	#mesh.mesh = surface.commit()
	#mesh.create_trimesh_collision()
	#mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	#mesh.add_to_group("NavSource")
	#add_child(mesh)
#
	#print(mesh)
