@tool
extends Node

@export var block: PackedScene
@export var noise: FastNoiseLite
@export var world_size: int = 64

var min_height: int = 10
var max_height: int = 30

func _ready() -> void:
    noise.seed = randi()
    _generate_world()

func _process(delta: float) -> void:
    pass

func _generate_world():
    for x in range(world_size):
        for z in range(world_size):
            var noise_val = noise.get_noise_2d(x, z)

            var blocks_in_stack = min_height + noise_val * max_height

            if noise_val < 0:
                blocks_in_stack = 1
            elif noise_val < .2:
                blocks_in_stack = 2
            elif noise_val < .4:
                blocks_in_stack = 4
            elif noise_val < 1:
                blocks_in_stack = 8

            for i in range(blocks_in_stack):
                var new_block = block.instantiate()
                new_block.position = Vector3(x, i, z)
                add_child(new_block)
