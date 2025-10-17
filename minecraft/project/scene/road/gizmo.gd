@tool
extends Node3D

## A callback function to be executed when the gizmo's transform is updated by the user.
## The callback will receive the new Transform3D as its only argument.
var update_callback: Callable

var _up_arrow_mesh: Mesh
var _forward_arrow_mesh: Mesh
var _right_arrow_mesh: Mesh

var _sphere_mesh: SphereMesh
var _sphere_instance: MeshInstance3D
var _up_arrow_instance: MeshInstance3D
var _forward_arrow_instance: MeshInstance3D
var _right_arrow_instance: MeshInstance3D
 
var _last_transform: Transform3D


func _init() -> void:
    # This ensures the gizmo runs its _process loop in the editor.
    set_process_mode(Node.PROCESS_MODE_PAUSABLE)

    _sphere_mesh = SphereMesh.new()
    _sphere_mesh.radius = 0.1
    _sphere_mesh.height = 0.2

    var up_mat := StandardMaterial3D.new()
    up_mat.albedo_color = Color.GREEN_YELLOW
    _up_arrow_mesh = _create_arrow_mesh()
    _up_arrow_mesh.surface_set_material(0, up_mat)

    var fwd_mat := StandardMaterial3D.new()
    fwd_mat.albedo_color = Color.BLUE
    _forward_arrow_mesh = _create_arrow_mesh()
    _forward_arrow_mesh.surface_set_material(0, fwd_mat)

    var right_mat := StandardMaterial3D.new()
    right_mat.albedo_color = Color.RED
    _right_arrow_mesh = _create_arrow_mesh()
    _right_arrow_mesh.surface_set_material(0, right_mat)

    # --- Create and arrange the visual components ---
    _sphere_instance = MeshInstance3D.new()
    _sphere_instance.mesh = _sphere_mesh
    _sphere_instance.name = "GizmoSphere"
    add_child(_sphere_instance)

    _up_arrow_instance = MeshInstance3D.new()
    _up_arrow_instance.mesh = _up_arrow_mesh
    _up_arrow_instance.name = "UpArrow"
    _up_arrow_instance.transform = Transform3D().rotated(Vector3.RIGHT, PI / 2.0)
    add_child(_up_arrow_instance)

    _forward_arrow_instance = MeshInstance3D.new()
    _forward_arrow_instance.mesh = _forward_arrow_mesh
    _forward_arrow_instance.name = "ForwardArrow"
    add_child(_forward_arrow_instance)

    _right_arrow_instance = MeshInstance3D.new()
    _right_arrow_instance.mesh = _right_arrow_mesh
    _right_arrow_instance.name = "RightArrow"
    _right_arrow_instance.transform = Transform3D().rotated(Vector3.UP, -PI / 2.0)
    add_child(_right_arrow_instance)


func _process(_delta: float) -> void:
    # Check if the user has moved, rotated, or scaled the gizmo in the editor.
    if global_transform != _last_transform:
        _last_transform = global_transform
        if update_callback.is_valid():
            update_callback.call(global_transform)

func set_gizmo_transform(new_transform: Transform3D) -> void:
    """Public method to update the gizmo's position and orientation from outside."""
    global_transform = new_transform
    _last_transform = new_transform

func _create_arrow_mesh() -> ArrayMesh:
    var st := SurfaceTool.new()
    st.begin(Mesh.PRIMITIVE_TRIANGLES)
    var cylinder := CylinderMesh.new()
    cylinder.height = 0.4
    cylinder.top_radius = 0.02
    cylinder.bottom_radius = 0.02
    # Rotate the Y-aligned cylinder to point along -Z
    var cyl_transform = Transform3D().rotated(Vector3.RIGHT, PI / 2.0).translated(Vector3(0, 0, -0.2))
    st.append_from(cylinder, 0, cyl_transform)

    var cone := BoxMesh.new()
    cone.size = Vector3(0.05, 0.05, 0.05) # Base radius and height
    var cone_transform = Transform3D().translated(Vector3(0, 0, -0.4))
    st.append_from(cone, 0, cone_transform)

    return st.commit()
