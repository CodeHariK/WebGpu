## Preview a move (from the tuning panel): the scene is reset, the move is pressed at a set target,
## and the game runs in fixed STEPs with the random numbers seeded — so every replay is the same,
## and scrubbing to time t just replays the first t seconds in one go (then holds still there).
##
## While a preview is on, BladesDemo asks steps() each frame how many fixed steps to run (none while
## it's paused), aims at `aim_override` instead of the mouse, and starts blade paths from the fixed
## hand anchor (the body's animated grip would differ from one replay to the next).
##
## Each move's recipe: what it aims at, and when its key is pressed (some moves need a second press:
## the lasso's throw, the rip's tugs).
class_name MovePreview
extends RefCounted

const STEP := 1.0 / 60.0
const MAX_TIME := 8.0 ## seconds: a preview stops here even if the move hasn't finished
const SEED := 7
const RECIPES := {
	"throw": {"target": "enemy", "presses": [0.0]},
	"lasso": {"target": "enemy", "presses": [0.0, 1.4]},
	"rip": {"target": "prop", "presses": [0.0, 0.8, 1.1, 1.4]},
	"rush": {"target": "post", "presses": [0.0]},
	"anchor": {"target": "ahead", "presses": [0.0]},
}
const DEFAULT_RECIPE := {"target": "enemy", "presses": [0.0]}

var active := false ## a preview is on (the scene belongs to it)
var playing := false ## running (else paused / scrubbed)
var speed := 1.0 ## playback speed (slow motion: fewer steps per real second)
var time := 0.0 ## seconds of the move played so far
var length := 0.0 ## the move's length, once it has played to the end (0: not yet)
var move := ""

var _demo: BladesDemo
var _accum := 0.0
var _pressed := 0 ## presses of the recipe done so far


func _init(demo: BladesDemo) -> void:
	_demo = demo


## Start the preview of `move_key` from the beginning and play it.
func play(move_key: String) -> void:
	if move_key != move:
		length = 0.0
	move = move_key
	_restart()
	playing = true


## Pause / go on.
func toggle() -> void:
	if active:
		playing = not playing and time < MAX_TIME


## Jump to `t` seconds: replay the start up to there in one go, then hold.
func scrub(t: float) -> void:
	if move == "":
		return
	_restart()
	playing = false
	for i in roundi(t / STEP): # whole steps, so a scrub lands on exactly the frame the play showed
		_step()


## Something it depends on changed (a slider): replay to the same moment, keep playing or paused.
func refresh() -> void:
	if not active:
		return
	var was_playing := playing
	scrub(time)
	playing = was_playing


## Leave the preview: back to the normal game.
func stop() -> void:
	active = false
	playing = false
	_demo.aim_override = Vector3.INF
	_demo._reset()


## BladesDemo, each frame: run the fixed steps due by now; the number run.
func steps(delta: float) -> int:
	if not playing:
		return 0
	_accum += delta * speed
	var count := 0
	while _accum >= STEP and playing:
		_accum -= STEP
		_step()
		count += 1
		if (time > 0.3 and _finished()) or time >= MAX_TIME:
			playing = false
			length = maxf(length, time)
	return count


func _step() -> void:
	var presses: Array = _recipe().presses
	while _pressed < presses.size() and time >= float(presses[_pressed]):
		_demo.press(move)
		_pressed += 1
	_demo.simulate(STEP)
	time += STEP


# Everything back to the start: same scene, same seed, the camera already in place, aimed.
func _restart() -> void:
	active = true
	seed(SEED)
	_demo._reset()
	_demo.hitstop = 0.0
	_demo.shake = 0.0
	_demo._queued = Callable()
	_demo._queued_name = ""
	var camera := _demo.camera
	var view := camera.global_transform # put back after: the editor camera stays where you put it
	var offset := camera.h_offset
	camera.h_offset = 0.0
	camera.position = _demo.player + BladesDemo.CAMERA_OFFSET # the game's view, so screen picking (rush) replays the same
	camera.look_at(camera.position - BladesDemo.CAMERA_OFFSET + Vector3(0, 0.5, 0))
	var aim := _target()
	_demo.aim_override = aim
	_demo.mouse = camera.unproject_position(aim)
	camera.global_transform = view
	camera.h_offset = offset
	var flat := Vector3(aim.x, 0.0, aim.z) - _demo.player
	if flat.length() > 0.3:
		_demo.facing = flat.normalized()
	_demo.body.reset()
	time = 0.0
	_accum = 0.0
	_pressed = 0


func _recipe() -> Dictionary:
	return RECIPES.get(move, DEFAULT_RECIPE)


func _target() -> Vector3:
	match _recipe().target:
		"prop": return RipMove.PROPS[0] + Vector3(0, 1, 0)
		"post": return RushMove.POSTS[0]
		"ahead": return _demo.player + Vector3(0, 1, -3)
	return _demo.enemies[1].position


# The move is over: blades home, no one carried, nothing waiting.
func _finished() -> bool:
	var moves := _demo.moves
	return _demo.both_idle() and not moves.rush.rushing and not moves.anchor.hauling \
			and moves.lasso.held < 0 and moves.rip.prop < 0 and moves.bind._binds.is_empty() \
			and _demo._queued_name == "" \
			and _pressed >= (_recipe().presses as Array).size()
