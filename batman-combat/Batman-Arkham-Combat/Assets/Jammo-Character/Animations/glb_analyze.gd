extends Node3D


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	# Since this script is attached to the Batman GLB model instance:
	# Let's search for an AnimationPlayer node child.
	var anim_player = _find_animation_player(self)
	if anim_player:
		print("--- Animations in " + name + " ---")
		var anim_list = anim_player.get_animation_list()
		for anim_name in anim_list:
			print("- " + anim_name)
		print("---------------------------------")
	else:
		print("--- No AnimationPlayer found on " + name + " or its children ---")
		
	# Dynamically check and spawn EnemyManager at scene root if needed
	var root = get_tree().root.get_child(0)
	if root and not root.has_node("EnemyManager"):
		var enemy_mgr = EnemyManager.new()
		enemy_mgr.name = "EnemyManager"
		enemy_mgr.add_to_group("enemy_manager")
		root.add_child(enemy_mgr)

func _find_animation_player(node: Node) -> AnimationPlayer:
	if node is AnimationPlayer:
		return node
	for child in node.get_children():
		var found = _find_animation_player(child)
		if found:
			return found
	return null

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass
