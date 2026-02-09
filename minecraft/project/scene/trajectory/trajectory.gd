extends MeshInstance3D

@export var curve_length := 20.0
@export var cross_section: PackedVector2Array
@export var albedo_texture_res: Texture2D
@export var circle_segments := 16
@export var close_shape := true
@export var tube_radius := 0.2
@export_range(0.0, 1.0) var pin := 1.0
@export var num_points := 256 # Must be a power of 2 for some GPU optimizations

var height_image: Image
var height_texture: ImageTexture

func _ready() -> void:
    line_mesh()


# Called when the node enters the scene tree for the first time.
func line_mesh() -> void:
    # 1. Create a base mesh by extruding a cross-section along a straight line.
    # If no cross-section is defined, create a default circle.
    if cross_section.is_empty():
        if close_shape:
            for i in range(circle_segments): # Don't add the last point, the index loop will close it.
                var angle = float(i) / circle_segments * TAU
                cross_section.append(Vector2(cos(angle), sin(angle)))
        else:
            for i in range(circle_segments + 1): # Create a full 360-degree arc as a polyline
                var angle = float(i) / circle_segments * TAU
                cross_section.append(Vector2(cos(angle), sin(angle)))

    var verts := PackedVector3Array()
    var uvs := PackedVector2Array()
    var indices := PackedInt32Array()

    var shape_point_count = cross_section.size()

    # Generate vertices and UVs
    for i in range(num_points):
        var path_progress = float(i) / (num_points - 1)
        var z_pos = - path_progress * curve_length

        for j in range(shape_point_count):
            var shape_progress = float(j) / (shape_point_count - 1)
            var shape_point = cross_section[j] * tube_radius

            verts.append(Vector3(shape_point.x, shape_point.y, z_pos))
            # U: along the length (path_progress), V: around the circumference (shape_progress)
            # This aligns textures to run along the tube's length.
            uvs.append(Vector2(shape_progress, path_progress))

    # Generate indices to form triangles
    for i in range(num_points - 1):
        var num_segments = shape_point_count - 1
        if close_shape:
            num_segments = shape_point_count

        for j in range(num_segments):
            var current_row = i * shape_point_count
            var next_row = (i + 1) * shape_point_count

            var p1: int
            var p2: int
            var p3: int
            var p4: int

            if close_shape:
                p1 = current_row + j
                p2 = current_row + (j + 1) % shape_point_count
                p3 = next_row + j
                p4 = next_row + (j + 1) % shape_point_count
            else:
                p1 = current_row + j
                p2 = current_row + j + 1
                p3 = next_row + j
                p4 = next_row + j + 1

            # Triangle 1
            indices.append(p1)
            indices.append(p3)
            indices.append(p2)
            
            # Triangle 2
            indices.append(p2)
            indices.append(p3)
            indices.append(p4)

    _create_mesh_from_data(verts, uvs, indices)


func _create_mesh_from_data(verts: PackedVector3Array, uvs: PackedVector2Array, indices: PackedInt32Array) -> void:
    var arrays := []
    arrays.resize(Mesh.ARRAY_MAX)
    arrays[Mesh.ARRAY_VERTEX] = verts
    arrays[Mesh.ARRAY_TEX_UV] = uvs
    arrays[Mesh.ARRAY_INDEX] = indices

    # Calculate the perimeter of the cross-section for aspect-correct UVs
    var shape_point_count = cross_section.size()
    var perimeter = 0.0
    if shape_point_count > 1:
        # Sum distances between each point in the cross-section
        for j in range(shape_point_count - 1):
            perimeter += cross_section[j].distance_to(cross_section[j + 1])
        # If the shape is closed, add the distance from the last point back to the first
        if close_shape:
            perimeter += cross_section[shape_point_count - 1].distance_to(cross_section[0])
    perimeter *= tube_radius

    var array_mesh := ArrayMesh.new()
    array_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)

    # Generate normals
    var surface_tool = SurfaceTool.new()
    surface_tool.create_from(array_mesh, 0) # 0 is the surface index
    surface_tool.generate_normals()
    array_mesh = surface_tool.commit()

    self.mesh = array_mesh
    _setup_shader_and_data(perimeter)


func _setup_shader_and_data(perimeter: float = 0.0) -> void:
    # 2. Create the image that will store our height data.
    # We use a floating-point format for precision. FORMAT_RGF has two channels (red, green).
    height_image = Image.create(num_points, 1, false, Image.FORMAT_RGF)

    # 3. Create a texture from that image to pass to the shader.
    height_texture = ImageTexture.create_from_image(height_image)

    # 4. Create and assign the ShaderMaterial.
    var mat := ShaderMaterial.new()
    mat.shader = load("res://scene/trajectory/trajectory.gdshader")
    # Pass the texture to the shader as a uniform.
    mat.set_shader_parameter("height_texture", height_texture)
    if albedo_texture_res:
        mat.set_shader_parameter("albedo_texture", albedo_texture_res)

    if perimeter == 0.0 and cross_section.size() > 1:
        # Recalculate perimeter if not provided (for Path3D case)
        for j in range(cross_section.size() - 1):
            perimeter += cross_section[j].distance_to(cross_section[j + 1])
        if close_shape:
            perimeter += cross_section[cross_section.size() - 1].distance_to(cross_section[0])
    perimeter *= tube_radius

    mat.set_shader_parameter("perimeter", perimeter * curve_length)

    self.material_override = mat


func _generate_curve_data() -> void:
    """Calculates the displacement values for the curve and updates the data texture."""
    var time := Time.get_unix_time_from_system() * 0.5
    
    # Update the height data in the image on the CPU.
    # This represents X/Y displacement from a straight line along the Z-axis.
    for i in range(num_points):
        var progress = float(i) / (num_points - 1)
        var phase = progress * TAU * 3.0 # Create a more interesting curve

        # Create an envelope to make the wave amplitude zero at the ends.
        # This prevents the stretching/pinning effect.
        var envelope = 1.0 - pin * pow(progress * 2.0 - 1.0, 2.0)

        var displacement_x = sin(time + phase) * envelope
        var displacement_y = cos(time + phase) * envelope
        height_image.set_pixel(i, 0, Color(displacement_x, displacement_y, 0))

    # Update the texture with the new image data. This sends it to the GPU.
    height_texture.update(height_image)


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta: float) -> void:
    _generate_curve_data()
