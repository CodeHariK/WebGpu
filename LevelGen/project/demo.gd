extends Node3D

var hello: Hello3D
var toggle : Button
	
# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	hello = $Hello3D
	toggle = $Control/Button
	
	toggle.connect("button_down", _on_button_button_down)


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	print(hello.spin_multiplier)
	pass



func _on_button_button_down() -> void:
	hello.toggle_spin()
	
