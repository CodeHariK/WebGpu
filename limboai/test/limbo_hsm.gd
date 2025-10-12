extends LimboHSM

@export var character: CharacterBody3D

@export var states : Dictionary[String, LimboState]

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
    _binding_setup()
    initialize(character)
    set_active(true)

func _binding_setup():
    add_transition(states["idle"], states["move"], "moving")
    add_transition(states["move"], states["idle"], "stopped")
