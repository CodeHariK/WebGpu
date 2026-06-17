extends Node

@export var player : Player
@export var anim_tree : AnimationTree
@export var skills_container : SkillsContainer
@export var movement_controller : MovementController
@export var camera_controller : CameraController

var last_movement_state : MovementState


func _on_pressed_primary_fire():
	skills_container.queue_primary_attack()

func _on_pressed_secondary_fire():
	skills_container.queue_secondary_attack()


func _on_activating_skill(active_skill : ActiveSkill):
	if is_skill_active():
		return
	
	if active_skill == null:
		return
	
	player.is_attacking = true
	
	movement_controller.acceleration = active_skill.acceleration
	movement_controller.dash(active_skill.dash_velocity, active_skill.dash_duration, active_skill.dash_delay)
	camera_controller.set_fov(active_skill.camera_fov)


func _on_completed_recovery():
	player.is_attacking = false
	movement_controller._on_set_movement_state(last_movement_state)
	camera_controller.set_fov(last_movement_state.camera_fov)


func _on_changed_movement_state(_movement_state : MovementState):
	last_movement_state = _movement_state


func is_skill_active() -> bool:
	return skills_container.attack_state != ActiveSkill.State.READY


func _on_switched_weapon(_weapon : Weapon):
	if skills_container != null:
		if skills_container.completed_recovery.is_connected(_on_completed_recovery):
			skills_container.completed_recovery.disconnect(_on_completed_recovery)
		if skills_container.activating_skill.is_connected(_on_activating_skill):
			skills_container.activating_skill.disconnect(_on_activating_skill)
	
	skills_container = _weapon.skills_container
	
	if skills_container != null:
		skills_container.set_anim_tree(anim_tree)
		skills_container.completed_recovery.connect(_on_completed_recovery)
		skills_container.activating_skill.connect(_on_activating_skill)
	
	
