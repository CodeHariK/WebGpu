extends CharacterState

func _update(delta: float) -> void:
    var velocity : Vector3 = move()    

    if velocity.is_equal_approx(Vector3.ZERO):
        dispatch("stopped", velocity)

    #print(character, "  ", blackboard.get_var(PlayerActions.jump), "  ", blackboard.get_var(PlayerActions.l_input_dir))
        
    if blackboard.get_var(PlayerActions.jump):
        #jump()
        character.velocity.y = -JUMP_VELOCITY
        #print(character.velocity)
    #
#func jump():
    #character.velocity.y = JUMP_VELOCITY
    #print(character.velocity)
