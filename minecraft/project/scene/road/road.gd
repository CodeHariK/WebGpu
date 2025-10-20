@tool
extends Node3D

@onready var ori_path: Path3D = $"."
@onready var path: Path3D = $DebugPath

enum CurveInterpolation {
    LINEAR,
    QUADRATIC,
    HERMITE,
    CARDINAL,
    CUBIC,
    BSPLINE
}

@export_group("Curve")
@export var curve_interpolation: CurveInterpolation = CurveInterpolation.HERMITE:
    set(value):
        curve_interpolation = value
        is_dirty = true

@export var tension: float = 3.0:
    set(value):
        tension = value
        is_dirty = true

@export_group("Planks")
@export var distance_between_planks = 1.0:
    set(value):
        distance_between_planks = value
        is_dirty = true

@export_group("Debug")
@export var show_debug_points = false:
    set(value):
        show_debug_points = value
        is_dirty = true

var is_dirty = false
var _debug_points: Array[MeshInstance3D]

# Called when the node enters the scene tree for the first time.
func _ready():
    is_dirty = true

func _process(delta):
    if is_dirty:
        # Clear existing debug meshes
        for cube in _debug_points:
            if is_instance_valid(cube):
                cube.queue_free()
        _debug_points.clear()

        if curve_interpolation == CurveInterpolation.LINEAR:
            var new_curve = Curve3D.new()
            var source_curve = ori_path.curve
            for i in range(source_curve.get_point_count()):
                var pos = source_curve.get_point_position(i)
                new_curve.add_point(pos, Vector3.ZERO, Vector3.ZERO)
            path.curve = new_curve

        ###--------
        elif curve_interpolation == CurveInterpolation.CUBIC:
            path.curve = ori_path.curve # Use the original curve directly
        
        ###--------
        elif curve_interpolation == CurveInterpolation.QUADRATIC:
            var new_curve = Curve3D.new()
            var source_curve = ori_path.curve
            for i in range(source_curve.get_point_count()):
                new_curve.add_point(source_curve.get_point_position(i), source_curve.get_point_in(i), source_curve.get_point_out(i))

            for i in range(new_curve.get_point_count() - 1):
                var p0 = new_curve.get_point_position(i)
                var p3 = new_curve.get_point_position(i + 1)
                var p1 = p0 + new_curve.get_point_out(i)
                var p2 = p3 + new_curve.get_point_in(i + 1)
                var mid = (p1 + p2) / 2.0
                new_curve.set_point_out(i, mid - p0)
                new_curve.set_point_in(i + 1, mid - p3)

            path.curve = new_curve
            
        ###--------
        elif curve_interpolation == CurveInterpolation.HERMITE:
            var new_curve = Curve3D.new()
            var source_curve = ori_path.curve
            if source_curve.get_point_count() > 0:
                # Copy points first
                for i in range(source_curve.get_point_count()):
                    new_curve.add_point(source_curve.get_point_position(i), source_curve.get_point_in(i), source_curve.get_point_out(i))

                # Adjust control points for Hermite interpolation
                for i in range(new_curve.get_point_count() - 1):
                    var vel = source_curve.get_point_out(i)
                    new_curve.set_point_in(i, -vel / tension)
                    new_curve.set_point_out(i, vel / tension)

            path.curve = new_curve

        ###--------
        elif curve_interpolation == CurveInterpolation.CARDINAL:
            var new_curve = Curve3D.new()
            var source_curve = ori_path.curve
            var point_count = source_curve.get_point_count()

            if point_count > 1:
                # 1. Copy all points from the source curve
                for i in range(point_count):
                    new_curve.add_point(source_curve.get_point_position(i))

                # 2. Calculate Catmull-Rom tangents for each point
                for i in range(point_count):
                    var p_prev = source_curve.get_point_position(max(0, i - 1))
                    var p_curr = source_curve.get_point_position(i)
                    var p_next = source_curve.get_point_position(min(point_count - 1, i + 1))
                    
                    var vel = (p_next - p_prev) * 0.5
                    
                    # For Catmull-Rom, the Bézier control points are derived from the tangents
                    # Outgoing control point is P_i + T_i / 3
                    # Incoming control point is P_i - T_i / 3
                    new_curve.set_point_out(i, vel / tension)
                    new_curve.set_point_in(i, -vel / tension)

            path.curve = new_curve


        _update_cubic_planks()
        _update_debug_cubic()
        is_dirty = false

func _on_curve_changed():
    is_dirty = true

func _update_debug_cubic():
    var curve = path.curve

    # No curve or not showing debug points
    if not show_debug_points or not curve:
        return

    for pointIndex in range(curve.get_point_count()):
        var point_pos = curve.get_point_position(pointIndex)
        var control_in = curve.get_point_in(pointIndex) + point_pos
        var control_out = curve.get_point_out(pointIndex) + point_pos

        # Box for the main point
        var box_instance = MeshInstance3D.new()
        box_instance.mesh = BoxMesh.new()
        box_instance.position = point_pos

        # Orient the box along the curve
        var offset = curve.get_closest_offset(point_pos)
        var pos = curve.sample_baked(offset, true)
        var up = curve.sample_baked_up_vector(offset, true)
        var forward = pos.direction_to(curve.sample_baked(offset + 0.1, true))
        box_instance.basis = Basis(forward.cross(up).normalized() * 0.5, up * 2.5, -forward * 1.5)
        add_child(box_instance)
        _debug_points.append(box_instance)

        # Spheres for control points
        var sphere_in_instance = MeshInstance3D.new()
        sphere_in_instance.mesh = SphereMesh.new()
        sphere_in_instance.mesh.radius = 0.25
        sphere_in_instance.position = control_in
        add_child(sphere_in_instance)
        _debug_points.append(sphere_in_instance)

        var sphere_out_instance = MeshInstance3D.new()
        sphere_out_instance.mesh = SphereMesh.new()
        sphere_out_instance.mesh.radius = 0.25
        sphere_out_instance.position = control_out
        add_child(sphere_out_instance)
        _debug_points.append(sphere_out_instance)

func _update_cubic_planks():
    var path_length: float = path.curve.get_baked_length()
    var count = floor(path_length / distance_between_planks)

    var curve = path.curve

    var mm: MultiMesh = $MultiMeshInstance3D.multimesh
    mm.instance_count = count
    var offset = distance_between_planks / 2.0
    for bakedPointIndex in range(0, count):
        var curve_distance = offset + distance_between_planks * bakedPointIndex
        var pos = curve.sample_baked(curve_distance, true)

        var mBasis = Basis()
        
        var up = curve.sample_baked_up_vector(curve_distance, true)
        var forward = pos.direction_to(curve.sample_baked(curve_distance + 0.1, true))

        mBasis.y = up
        mBasis.x = forward.cross(up).normalized()
        mBasis.z = - forward
        
        var mTransform = Transform3D(mBasis, pos)
        mm.set_instance_transform(bakedPointIndex, mTransform)
