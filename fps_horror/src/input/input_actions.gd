## All the game's input actions, made in code so they live in one readable place (step 11 adds
## touch controls that press the same actions).
##   move_forward / move_back / move_left / move_right   WASD or arrows
##   jump    Space        sprint   Shift        crouch   Ctrl or C (hold)
##   fire    left mouse   aim      right mouse (hold)    reload   R
##   release_mouse   Esc (click the game to grab it again)
extends RefCounted

const KEYS := {
	"move_forward": [KEY_W, KEY_UP],
	"move_back": [KEY_S, KEY_DOWN],
	"move_left": [KEY_A, KEY_LEFT],
	"move_right": [KEY_D, KEY_RIGHT],
	"jump": [KEY_SPACE],
	"sprint": [KEY_SHIFT],
	"crouch": [KEY_CTRL, KEY_C],
	"reload": [KEY_R],
	"release_mouse": [KEY_ESCAPE],
}
const MOUSE := {
	"fire": MOUSE_BUTTON_LEFT,
	"aim": MOUSE_BUTTON_RIGHT,
}


## Add any action that doesn't exist yet (safe to call more than once).
static func ensure() -> void:
	for action: String in KEYS:
		if _add(action):
			for key: Key in KEYS[action]:
				var event := InputEventKey.new()
				event.physical_keycode = key
				InputMap.action_add_event(action, event)
	for action: String in MOUSE:
		if _add(action):
			var event := InputEventMouseButton.new()
			event.button_index = MOUSE[action]
			InputMap.action_add_event(action, event)


static func _add(action: String) -> bool:
	if InputMap.has_action(action):
		return false
	InputMap.add_action(action)
	return true
