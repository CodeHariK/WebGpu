extends CharacterBody3D

@export var animation_player: AnimationPlayer

var input_dir: Vector2
var jump : bool

@export var limbo_hsm: LimboHSM
var board: Blackboard

func _ready() -> void:
    board = limbo_hsm.blackboard
    
    board.bind_var_to_property(PlayerActions.l_input_dir, self, PlayerActions.l_input_dir, true)
    board.bind_var_to_property(PlayerActions.jump, self, PlayerActions.jump, true)

func _physics_process(delta: float) -> void:

    if not is_on_floor():
        velocity += get_gravity() * delta

    input_dir = Input.get_vector(PlayerActions.move_left, PlayerActions.move_right, 
                                 PlayerActions.move_up, PlayerActions.move_down)
    

func _unhandled_input(event: InputEvent) -> void:
    if event.is_action_pressed(PlayerActions.jump):
        jump = true
    elif event.is_action_released(PlayerActions.jump):
        jump = false    
