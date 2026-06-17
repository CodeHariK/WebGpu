@icon("mouse_left.png")
extends ComboNode
class_name PrimaryAttack

func _ready():
	var parent = get_parent()
	if parent != null:
		if parent is ComboNode:
			parent.next_primary = self
		elif parent is ComboTree:
			parent.first_primary = self
