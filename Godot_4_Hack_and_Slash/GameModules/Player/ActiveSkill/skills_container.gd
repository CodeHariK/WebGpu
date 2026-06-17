extends Node3D
class_name SkillsContainer

signal activating_skill
signal completed_recovery

@export var combo_tree : ComboTree
@export var active_skills : Array[ActiveSkill]

@onready var current_active_skill : ActiveSkill = active_skills[current_attack_id]
var current_attack_id : int = 0

var anim_tree : AnimationTree
var attack_transition_node : AnimationNodeTransition
var attack_anim_node : AnimationNodeAnimation
var attack_buffer : AnimationNodeBlendTree
var attack_buffer_id : int = 0

var windup_timer : Timer = Timer.new()
var delivery_timer : Timer = Timer.new()
var recovery_timer : Timer = Timer.new()
var end_timer : Timer = Timer.new()

var attack_state : ActiveSkill.State = ActiveSkill.State.READY


func _ready():
	windup_timer.timeout.connect(_on_windup_timer_timeout)
	delivery_timer.timeout.connect(_on_delivery_timer_timeout)
	recovery_timer.timeout.connect(_on_recovery_timer_timeout)
	end_timer.timeout.connect(_on_end_timer_timeout)
	windup_timer.one_shot = true
	delivery_timer.one_shot = true
	recovery_timer.one_shot = true
	end_timer.one_shot = true
	add_child(windup_timer)
	add_child(delivery_timer)
	add_child(recovery_timer)
	add_child(end_timer)


func set_anim_tree(_anim_tree : AnimationTree):
	anim_tree = _anim_tree
	attack_transition_node = anim_tree.tree_root.get("nodes/attack_transition/node")


func switch_attack_buffer(attack_anim_name : StringName):
	attack_buffer_id = 1 - attack_buffer_id
	attack_buffer = anim_tree.tree_root.get("nodes/attack_buffer_" + str(attack_buffer_id) + "/node")
	attack_anim_node = attack_buffer.get_node("Animation")
	attack_anim_node.animation = attack_anim_name


func set_attack_buffer_timescale(value : float):
	anim_tree["parameters/attack_buffer_" + str(attack_buffer_id) + "/TimeScale/scale"] = value


func set_attack_transition(value : String):
	anim_tree["parameters/attack_transition/transition_request"] = value


func get_current_skill() -> ActiveSkill:
	return active_skills[current_attack_id]


func queue_primary_attack():
	if combo_tree.combo_queue.is_empty():
		start_first_attack(combo_tree.first_primary)
	else:
		var last_node : ComboNode = combo_tree.combo_queue.back()
		if last_node.next_primary != null:
			combo_tree.combo_queue.push_back(last_node.next_primary)


func queue_secondary_attack():
	if combo_tree.combo_queue.is_empty():
		start_first_attack(combo_tree.first_secondary)
	else:
		var last_node : ComboNode = combo_tree.combo_queue.back()
		if last_node.next_secondary != null:
			combo_tree.combo_queue.push_back(last_node.next_secondary)


func start_first_attack(combo_node : ComboNode):
	combo_tree.combo_queue.push_back(combo_node)
	current_active_skill = combo_node.active_skill
	activate_skill()


func activate_skill():
	activating_skill.emit(current_active_skill)
	windup_timer.stop()
	delivery_timer.stop()
	recovery_timer.stop()
	end_timer.stop()

	switch_attack_buffer(current_active_skill.animation_name)
	windup()


func windup():
	attack_state = ActiveSkill.State.WINDING_UP
	attack_transition_node.xfade_time = current_active_skill.windup_duration
	set_attack_transition("attack_buffer_" + str(attack_buffer_id))
	set_attack_buffer_timescale(0)
	windup_timer.start(current_active_skill.windup_duration)
	stop_vfx()

func _on_windup_timer_timeout():
	deliver()


func deliver():
	attack_state = ActiveSkill.State.DELIVERING
	set_attack_buffer_timescale(1)
	delivery_timer.start(current_active_skill.delivery_duration)
	play_vfx()

func _on_delivery_timer_timeout():
	recover()


func recover():
	attack_state = ActiveSkill.State.RECOVERING
	recovery_timer.start(current_active_skill.recovery_duration)
	stop_vfx()


func _on_recovery_timer_timeout():
	attack_state = ActiveSkill.State.READY
	completed_recovery.emit()
	
	combo_tree.combo_queue.pop_front()
	
	if combo_tree.combo_queue.is_empty():
		end_timer.start(0.05)
	else:
		await get_tree().physics_frame
		await get_tree().physics_frame
		var next_combo_node : ComboNode = combo_tree.combo_queue.front()
		current_active_skill = next_combo_node.active_skill
		activate_skill()


func _on_end_timer_timeout():
	set_attack_transition("end_attack")


func play_vfx():
	await get_tree().create_timer(current_active_skill.vfx_delay).timeout
	current_active_skill.vfx.show()

	await get_tree().create_timer(current_active_skill.vfx_duration).timeout
	stop_vfx()


func stop_vfx():
	current_active_skill.vfx.hide()
	current_active_skill.vfx.restart()
