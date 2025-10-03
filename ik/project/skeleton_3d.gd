extends Skeleton3D


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
    for i in range(get_bone_count()):
        printt(i, get_bone_name(i))


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
    pass
