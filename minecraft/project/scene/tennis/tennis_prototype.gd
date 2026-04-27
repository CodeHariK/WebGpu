extends Node3D

@onready var game_manager = $TennisGameManager
@onready var player = $TennisPlayer
@onready var ai = $TennisAI
@onready var ball = $TennisBall

var was_swinging = false

var power_label: Label

func _ready():
	# Link the components
	game_manager.set_player(player)
	game_manager.set_ai(ai)
	game_manager.set_ball(ball)
	
	ai.set_ball(ball)
	
	# Create Power UI
	power_label = Label.new()
	add_child(power_label)
	power_label.position = Vector2(20, 20)
	power_label.add_theme_font_size_override("font_size", 24)
	
	# Start the game
	game_manager.start_serve()

func _physics_process(delta):
	var input_dir = Vector2.ZERO
	
	if Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP):
		input_dir.y -= 1
	if Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN):
		input_dir.y += 1
	if Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT):
		input_dir.x -= 1
	if Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT):
		input_dir.x += 1
		
	player.move(input_dir)
	player.set_aim_direction(input_dir)
	
	var is_swinging = Input.is_key_pressed(KEY_SPACE) or Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
	
	if is_swinging and not was_swinging:
		player.start_charge()
	elif not is_swinging and was_swinging:
		player.release_charge()
		
	was_swinging = is_swinging
	
	# Update UI
	if power_label and player.has_method("get_charge_power"):
		var power = player.get_charge_power()
		var state = player.get_state()
		var label_text = ""
		
		if state == 3: # SERVING
			label_text = "State: SERVING\nHold Space to Charge Serve\nAim with WASD\n"
			
		if power > 0:
			var bars = int(power * 20.0)
			var bar_str = ""
			for i in range(bars):
				bar_str += "|"
			label_text += "Power: [" + bar_str.rpad(20) + "]"
			
		power_label.text = label_text
