extends RigidBody3D

@onready var rigid_body_3d: RigidBody3D = $"."
@onready var animation_player: AnimationPlayer = $sophia/AnimationPlayer
@onready var animation_tree: AnimationTree = $AnimationTree
"parameters/StateMachine/Move/Add3/add_amount"

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass
