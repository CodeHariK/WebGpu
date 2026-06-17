extends Node
class_name Weapon

@export var weapon_name : String
@export var equip_duration : float = 0.5
@export var skills_container : SkillsContainer
@export var visual : Node3D
@export var stance_animations : Dictionary

func show_visual(value : bool):
	if visual == null:
		return
	
	visual.visible = value
