@tool
extends Node3D

const GizmoScene = preload("res://scene/road/gizmo.gd")


enum GizmoMode {MOVE, ROTATE, SCALE}

@export var gizmo_mode: GizmoMode = GizmoMode.MOVE:
    set(v):
        gizmo_mode = v
        _update_marker_materials()

@export var path: Path3D:
    set(v):
        path = v
        if Engine.is_editor_hint():
            _update_path_connection()

# This creates a button in the inspector. When you click it, `_generate_roads` is called.
@export var generate: bool = false:
    set(v):
        if v:
            generate_markers()
            generate = false # Set back to false to make it a one-shot button

@export var clear: bool = false:
    set(v):
        if v:
            _clear_markers()
            clear = false

var _markers: Array[Node3D] = []
var _point_scales: Array[Vector3] = []

var _move_material := StandardMaterial3D.new()
var _rotate_material := StandardMaterial3D.new()
var _scale_material := StandardMaterial3D.new()

func _enter_tree():
    # Create colored arrow meshes once
    # Create materials for different gizmo modes
    _move_material.albedo_color = Color.WHITE
    _rotate_material.albedo_color = Color.YELLOW
    _scale_material.albedo_color = Color.ORANGE_RED

    _update_path_connection()

func _clear_markers():
    for marker in _markers:
        if is_instance_valid(marker):
            marker.queue_free()
    _markers.clear()


func generate_markers():
    _clear_markers()

    if not is_instance_valid(path):
        print("Path3D not assigned.")
        return
    var curve: Curve3D = path.curve
    if not is_instance_valid(curve):
        print("Path3D does not contain a valid curve.")
        return

    var point_count = curve.get_point_count()
    _point_scales.resize(point_count)
    _point_scales.fill(Vector3.ONE)

    print("Generating markers for %d control points." % point_count)

    for i in range(point_count):
        var point_pos: Vector3 = curve.get_point_position(i)

        # Create an instance of our new Gizmo node
        var marker = GizmoScene.new()
        marker.name = "PointMarker_%d" % i
        add_child(marker)
        if Engine.is_editor_hint():
            marker.owner = get_tree().edited_scene_root

        var transform = _get_transform_for_point(i)
        marker.set_gizmo_transform(transform)
        # Set the callback to our update function, passing the point index
        marker.update_callback = Callable(self, "_on_gizmo_updated").bind(i)
        _markers.append(marker)
    
    _update_marker_materials()

func _get_transform_for_point(index: int) -> Transform3D:
    if not is_instance_valid(path) or not is_instance_valid(path.curve):
        return Transform3D()

    var curve := path.curve
    var point_count = curve.get_point_count()
    var point_pos = curve.get_point_position(index)
    var offset = curve.get_closest_offset(point_pos)

    var origin = curve.sample_baked(offset)
    var up_vector = curve.sample_baked_up_vector(offset)

    var look_at_pos: Vector3
    if index < point_count - 1:
        look_at_pos = curve.sample_baked(offset + 0.01)
    else:
        var prev_offset = curve.get_closest_offset(curve.get_point_position(index - 1))
        var prev_pos = curve.sample_baked(prev_offset)
        look_at_pos = origin + (origin - prev_pos).normalized()

    return Transform3D().looking_at(look_at_pos, up_vector).translated_local(origin)

func _process(_delta):
    if not Engine.is_editor_hint() or not is_instance_valid(path):
        return

    var curve: Curve3D = path.curve
    if not is_instance_valid(curve):
        return

    # In SCALE mode, we still need to check the scale of the gizmo nodes directly
    if gizmo_mode == GizmoMode.SCALE:
        for i in range(_markers.size()):
            var marker = _markers[i]
            if is_instance_valid(marker) and marker.scale != _point_scales[i]:
                _point_scales[i] = marker.scale
                print("Updated scale for point %d to %s" % [i, marker.scale])

func _on_gizmo_updated(new_transform: Transform3D, index: int):
    """This function is called by the Gizmo node when it's updated by the user."""
    if not is_instance_valid(path) or not is_instance_valid(path.curve):
        return

    if gizmo_mode == GizmoMode.MOVE or gizmo_mode == GizmoMode.ROTATE:
        path.curve.set_point_position(index, new_transform.origin)
        path.curve.set_point_up_vector(index, new_transform.basis.y.normalized())
        # Let Godot know the curve has changed so it rebakes
        path.curve.emit_signal("changed")

func _update_marker_materials():
    var active_material: Material
    match gizmo_mode:
        GizmoMode.MOVE:
            active_material = _move_material
        GizmoMode.ROTATE:
            active_material = _rotate_material
        GizmoMode.SCALE:
            active_material = _scale_material
    
    for marker in _markers:
        if is_instance_valid(marker):
            marker.set_material_override(active_material)


func _on_path_curve_changed():
    # If the curve is changed directly, regenerate markers to match
    print("Path curve changed, regenerating markers.")
    generate_markers()

func _update_path_connection():
    if is_instance_valid(path) and is_instance_valid(path.curve):
        if not path.curve.is_connected("changed", Callable(self, "_on_path_curve_changed")):
            path.curve.connect("changed", Callable(self, "_on_path_curve_changed"))
    # Disconnect from old path if it exists
    # (This part is more complex and omitted for clarity, but important for robust tools)
