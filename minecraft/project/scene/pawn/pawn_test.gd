extends Node3D
## Spring-body character test bed (SpringCharacter on PhysicsPawn).
##
##   make run_pawn         -> interactive: drive with WASD / arrows, jump = Space.
##   make run_pawn ARGS=--pawntest  (or the headless smoke test) -> scripted
##       settle / move / jump check that prints results and quits.
##
## A TPS GameCamera follows the capsule so you can watch it. Movement is
## world-relative for now (camera-relative is the next step).

@onready var _char: Node3D = $Character
var _scripted := false
var _t := 0.0
var _settle_y := 0.0
var _peak_y := -1000.0
var _start_x := 0.0
var _phase := ""

func _ready() -> void:
	_scripted = OS.get_cmdline_args().has("--pawntest") or OS.get_cmdline_user_args().has("--pawntest")

func _physics_process(delta: float) -> void:
	if not _scripted:
		return # interactive play: leave input to the keyboard
	_t += delta
	var pos: Vector3 = _char.global_position
	_peak_y = max(_peak_y, pos.y)

	if _t < 1.2:
		_phase = "settle"
		_settle_y = pos.y
		_start_x = pos.x
	elif _t < 2.4:
		if _phase != "move":
			_phase = "move"
			print("[pawn] settled y=%.3f grounded=%s" % [_settle_y, str(_char.is_grounded())])
		Input.action_press("move_right")
	elif _t < 3.6:
		if _phase != "jump":
			_phase = "jump"
			Input.action_release("move_right")
			print("[pawn] after move x=%.3f (start %.3f)" % [pos.x, _start_x])
			_peak_y = pos.y
			Input.action_press("jump")
		else:
			Input.action_release("jump")
	else:
		print("[pawn] jump peak y=%.3f (rise %.3f from settle %.3f)" % [_peak_y, _peak_y - _settle_y, _settle_y])
		print("[pawn] final pos=", _char.global_position, " grounded=", _char.is_grounded())
		get_tree().quit()
