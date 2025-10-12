extends Label

@export var limbo_hsm : LimboHSM :
    set(value):
        if limbo_hsm != null:
            limbo_hsm.active_state_changed.disconnect(_on_active_state_change)
            
        limbo_hsm = value
        
        if limbo_hsm != null:
            var current_state = limbo_hsm.get_active_state()
            if current_state != null:
                text = current_state.name
            limbo_hsm.active_state_changed.connect(_on_active_state_change)

func _on_active_state_change(current: LimboState, previous: LimboState) -> void:
    text = current.name
