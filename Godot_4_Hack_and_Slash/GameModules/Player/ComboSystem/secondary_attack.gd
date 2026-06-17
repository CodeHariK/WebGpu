@icon("mouse_right.png")
extends ComboNode
class_name SecondaryAttack

func _ready():
	var parent = get_parent()
	if parent != null:
		if parent is ComboNode:
			parent.next_secondary = self
		elif parent is ComboTree:
			parent.first_secondary = self
