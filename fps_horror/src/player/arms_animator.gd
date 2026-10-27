## The arms' animation states: an AnimationTree state machine over the 8 clips in
## ArmsAnimations.gltf, wired like the original (ArmsPrefab.tscn):
##
##   Start → LowIdle ⇄ (LowToHigh / HighToLow) ⇄ HighIdle
##   LowIdle  → LowFire  → LowIdle         HighIdle → HighFire  → HighIdle
##   LowIdle  → LowReload → LowIdle        HighIdle → HighReload → LowIdle (reloading lowers the gun)
##
## "Low" is the gun held down at the hip, "High" is aiming down the sights. One-shot clips go back
## to idle by themselves when they end. The idle clips are set to loop here (the glTF doesn't).
extends RefCounted

const CLIPS := {
	&"LowIdle": &"v2_idlelow_004",
	&"HighIdle": &"v2_idlehigh_004",
	&"LowToHigh": &"v2_lowtohigh_004",
	&"HighToLow": &"v2_hightolow_005",
	&"LowFire": &"v2_lowfire_004",
	&"HighFire": &"v2_highfire_004",
	&"LowReload": &"v2_lowreload_006",
	&"HighReload": &"v2_highreload_004",
}
const LOOPING := [&"LowIdle", &"HighIdle"]
const BUSY := [&"LowFire", &"HighFire", &"LowReload", &"HighReload", &"LowToHigh", &"HighToLow"]

var tree := AnimationTree.new()
var playback: AnimationNodeStateMachinePlayback


## `rig` is the instanced ArmsAnimations.gltf (it holds the AnimationPlayer).
func _init(rig: Node) -> void:
	var player: AnimationPlayer = rig.find_children("*", "AnimationPlayer", true, false)[0]
	player.stop()
	for state in LOOPING:
		player.get_animation(CLIPS[state]).loop_mode = Animation.LOOP_LINEAR
	tree.tree_root = _state_machine()
	tree.name = "ArmsAnimationTree"
	rig.add_child(tree)
	tree.anim_player = tree.get_path_to(player)
	tree.active = true
	playback = tree.get("parameters/playback")


func _state_machine() -> AnimationNodeStateMachine:
	var sm := AnimationNodeStateMachine.new()
	for state: StringName in CLIPS:
		var clip := AnimationNodeAnimation.new()
		clip.animation = CLIPS[state]
		sm.add_node(state, clip)
	_link(sm, &"Start", &"LowIdle", 0.0, true)
	_link(sm, &"LowIdle", &"LowToHigh", 0.1)
	_link(sm, &"LowToHigh", &"HighIdle", 0.1, true)
	_link(sm, &"HighIdle", &"HighToLow", 0.1)
	_link(sm, &"HighToLow", &"LowIdle", 0.1, true)
	_link(sm, &"LowIdle", &"LowFire", 0.05)
	_link(sm, &"LowFire", &"LowIdle", 0.1, true)
	_link(sm, &"HighIdle", &"HighFire", 0.05)
	_link(sm, &"HighFire", &"HighIdle", 0.1, true)
	_link(sm, &"LowIdle", &"LowReload", 0.1)
	_link(sm, &"LowReload", &"LowIdle", 0.2, true)
	_link(sm, &"HighIdle", &"HighReload", 0.1)
	_link(sm, &"HighReload", &"LowIdle", 0.2, true)
	return sm


## `at_end`: leave by itself when the clip finishes. Otherwise only when travel() asks.
func _link(sm: AnimationNodeStateMachine, from: StringName, to: StringName, fade: float, at_end := false) -> void:
	var t := AnimationNodeStateMachineTransition.new()
	t.xfade_time = fade
	if at_end:
		t.switch_mode = AnimationNodeStateMachineTransition.SWITCH_MODE_AT_END
		t.advance_mode = AnimationNodeStateMachineTransition.ADVANCE_MODE_AUTO
	sm.add_transition(from, to, t)


func current() -> StringName:
	return playback.get_current_node()


func busy() -> bool:
	return current() in BUSY


## Every frame: head for the idle that matches whether we want to aim (unless mid-clip).
func update(want_aim: bool) -> void:
	if busy():
		return
	var target := &"HighIdle" if want_aim else &"LowIdle"
	if current() != target:
		playback.travel(target)


## Play the fire clip for the current stance (restarts it when firing again mid-shot).
## Returns false when the arms can't fire right now (reloading, raising, lowering).
func fire() -> bool:
	match current():
		&"LowIdle", &"LowFire":
			_play(&"LowFire")
		&"HighIdle", &"HighFire":
			_play(&"HighFire")
		_:
			return false
	return true


func reload() -> bool:
	match current():
		&"LowIdle", &"LowFire":
			playback.travel(&"LowReload")
		&"HighIdle", &"HighFire":
			playback.travel(&"HighReload")
		_:
			return false
	return true


func _play(state: StringName) -> void:
	if current() == state:
		playback.start(state, true)
	else:
		playback.travel(state)
