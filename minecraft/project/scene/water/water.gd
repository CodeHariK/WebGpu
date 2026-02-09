extends MeshInstance3D

var material: ShaderMaterial

var noise: Image
var height_scale = 0.5
var wave_speed = 0.1
var wave_scale = 0.1

var time: float

@onready var debug_water: MeshInstance3D = $Water2
var mesh_data_tool := MeshDataTool.new()


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
    material = mesh.surface_get_material(0)

    noise = material.get_shader_parameter("HEIGHT_NOISE").noise.get_seamless_image(256, 256)
    height_scale = material.get_shader_parameter("HEIGHT_SCALE")
    wave_speed = material.get_shader_parameter("WAVE_SPEED")
    wave_scale = material.get_shader_parameter("WAVE_SCALE")
    
    # Convert the debug mesh to an ArrayMesh if it's a primitive.
    # This is necessary because primitive meshes (like PlaneMesh) cannot be modified at runtime.
    if debug_water and debug_water.mesh is PrimitiveMesh:
        var array_mesh := ArrayMesh.new()
        array_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, debug_water.mesh.get_mesh_arrays())
        debug_water.mesh = array_mesh


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
    time += delta
    material.set_shader_parameter("WAVE_TIME", time)
    
    _update_debug_water_mesh()


func get_height(world_position: Vector3) -> float:
    var uv_x = wrapf(world_position.x * wave_scale + time * wave_speed, 0, 1)
    var uv_y = wrapf(world_position.z * wave_scale + time * wave_speed, 0, 1)

    var pixel_pos = Vector2(uv_x * noise.get_width(), uv_y * noise.get_height())
    return global_position.y + noise.get_pixelv(pixel_pos).r * height_scale;


func _update_debug_water_mesh() -> void:
    if not debug_water or not debug_water.mesh:
        return

    # Create a handle to the mesh data
    mesh_data_tool.create_from_surface(debug_water.mesh, 0)

    # Iterate through each vertex, get its world position, and update its height
    for i in range(mesh_data_tool.get_vertex_count()):
        var local_vertex: Vector3 = mesh_data_tool.get_vertex(i)
        var world_vertex: Vector3 = debug_water.to_global(local_vertex)
        world_vertex.y = get_height(world_vertex)
        mesh_data_tool.set_vertex(i, debug_water.to_local(world_vertex))

    # Clear the old mesh surface and commit the updated data to create the new one
    debug_water.mesh.clear_surfaces()
    mesh_data_tool.commit_to_surface(debug_water.mesh)
