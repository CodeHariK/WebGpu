extends Node3D

@export var default_cube: MeshInstance3D
@export var noise: FastNoiseLite
@export var world_size: Vector3 = Vector3(16, 16, 16)
@export var cut_off: float = 0.5

func _ready() -> void:
    noise.seed = randi()
    _generate_world()

func _generate_world():
    for x in range(world_size.x):
        for y in range(world_size.y):
            for z in range(world_size.z):
                var noise_val = noise.get_noise_3d(x, y, z)

                if noise_val > cut_off:
                    var new_block = default_cube.duplicate()
                    new_block.position = Vector3(x, y, z)
                    add_child(new_block)

    remove_child(default_cube)
