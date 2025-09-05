extends Node3D

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
    pass

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
    do_raycast()

func do_raycast():
    var camera = get_viewport().get_camera_3d()
    if camera == null:
        return
    var origin: Vector3 = camera.global_transform.origin
    var direction: Vector3 = - camera.global_transform.basis.z # forward
    var end: Vector3 = origin + direction * 100.0

    var space_state := get_world_3d().direct_space_state
    if not space_state:
        return

    var params := PhysicsRayQueryParameters3D.create(origin, end)
    var result: Dictionary = space_state.intersect_ray(params)

    if result.size() > 0:
        var hit_pos: Vector3 = result["position"]
        var hit_normal: Vector3 = result["normal"]
        var collider: Object = result["collider"]
        
        var normal_end = hit_pos + hit_normal * 0.5
        DebugDraw3D.draw_line(hit_pos, normal_end, Color.GREEN)
        DebugDraw3D.draw_box(
            normal_end,
            Quaternion(),
            Vector3(0.05, 0.05, 0.05),
            Color.GREEN
        )

        print("Hit at: ", hit_pos)
        print("Normal: ", hit_normal)

        if collider is Node3D:
            print("Hit node: ", collider.name)
            if collider is MeshInstance3D:
                var mat: Material = collider.get_active_material(0)
                if mat:
                    print("Material: ", mat.resource_name)
