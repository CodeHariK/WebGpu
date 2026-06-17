extends Node
class_name WeaponSwitcher

signal switched_weapon(weapon : Weapon)

@export var weapons : Dictionary
@export var weapon_switch_timer : Timer

var current_weapon_name : String = "holster"


func _ready():
	switch_weapon("holster")


func _input(event):
	
	if not can_switch_weapon():
		return
	
	if event.is_action_pressed("holster"):
		switch_weapon("holster")
		
	if event.is_action_pressed("weapon_1"):
		switch_weapon("weapon_1")
		
	if event.is_action_pressed("weapon_2"):
		switch_weapon("weapon_2")


func can_switch_weapon() -> bool:
	var value = true
	
	if get_current_weapon().skills_container != null:
		if get_current_weapon().skills_container.attack_state != ActiveSkill.State.READY:
			value = false
	
	if is_switching():
		value = false
	
	return value


func is_switching() -> bool:
	return weapon_switch_timer.time_left > 0


func switch_weapon(_name : String):
	if current_weapon_name == _name:
		_name = "holster"
	
	weapon_switch_timer.start(get_current_weapon().equip_duration)
	
	var previous_weapon : Weapon = get_current_weapon()
	current_weapon_name = _name
	var current_weapon : Weapon = get_current_weapon()
	switch_weapon_visual(previous_weapon, current_weapon)
	switched_weapon.emit(get_current_weapon())


func switch_weapon_visual(previous_weapon : Weapon, current_weapon : Weapon):
	await get_tree().create_timer(previous_weapon.equip_duration / 2).timeout
	previous_weapon.show_visual(false)
	current_weapon.show_visual(true)


func get_current_weapon() -> Weapon:
	return get_node(weapons[current_weapon_name])


func get_skills_container() -> SkillsContainer:
	return get_current_weapon().skills_container
