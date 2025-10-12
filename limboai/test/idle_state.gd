extends CharacterState

func _update(delta: float) -> void:
    var velocity : Vector3 = move()    

    if not velocity.is_equal_approx(Vector3.ZERO):
        dispatch("moving", velocity)
