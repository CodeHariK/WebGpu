## All the game's input actions, made in code so they live in one readable place (step 11 adds
## touch controls that press the same actions).
##   move_forward / move_back / move_left / move_right   WASD or arrows
##   jump    Space        sprint   Shift        crouch   Ctrl or C (hold)
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
	"release_mouse": [KEY_ESCAPE],
}


## Add any action that doesn't exist yet (safe to call more than once).
static func ensure() -> void:
	for action: String in KEYS:
		if InputMap.has_action(action):
			continue
		InputMap.add_action(action)
		for key: Key in KEYS[action]:
			var event := InputEventKey.new()
			event.physical_keycode = key
			InputMap.action_add_event(action, event)
