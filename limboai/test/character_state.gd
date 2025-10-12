class_name CharacterState
extends LimboState

const SPEED = 5.0
const JUMP_VELOCITY = 4.5

@export var animation_name : StringName

var character: CharacterBody3D

func _enter() -> void:
    character = agent as CharacterBody3D
    var player = agent.animation_player as AnimationPlayer
    player.play(animation_name)

func move() -> Vector3:
    var input_dir = blackboard.get_var(PlayerActions.l_input_dir)
    
    print(character, "  ", blackboard.get_var(PlayerActions.jump), "  ", blackboard.get_var(PlayerActions.l_input_dir))
    
    var direction := (character.transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()

    if direction:
        character.velocity.x = direction.x * SPEED
        character.velocity.z = direction.z * SPEED
    else:
        character.velocity.x = move_toward(character.velocity.x, 0, SPEED)
        character.velocity.z = move_toward(character.velocity.z, 0, SPEED)

    character.move_and_slide()
    return character.velocity
