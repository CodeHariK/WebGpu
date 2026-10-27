## Step 6 — the enemy's body: a rolling ball that hunts you round the level. Four limb points
## (small markers: pale hands, red feet) step in turn and every step kicks the ball along its
## route; it jumps onto walls and the ceiling now and then. No cultist on it yet (step 7) and no
## attacks (step 8): it just follows you.
##   P route lines on/off · L lamps · F flashlight · plus step 4: LMB fire (shots knock it) …
##
## The navmesh is baked when the level loads (src/level/nav_bake.gd). The chase here is a stand-in
## for step 8's AI: re-plan to the player whenever they've moved 3 m (the original's rule).
##
## Test run: Godot --path . res://demos/step06_enemy.tscn -- --shot  → shots/step06_*.png
##   the camera follows the ball (moves named "watch …"): it crosses the hall, follows you into
##   the mine, climbs the stairs to the platform, and gets shot
extends "res://demos/step05_flashlight.gd"

const NavBake := preload("res://src/level/nav_bake.gd")
const PathDebug := preload("res://src/enemy/path_debug.gd")
const ENEMY := preload("res://scenes/enemy.tscn")
const ENEMY_SPAWN := Vector3(10, 1.0, -12)
const RETARGET := 3.0  ## re-plan once the player is this far from the last target (m)
const GROUNDED := ["in the air", "on the floor", "on a wall/ceiling"]

## [name, teleport (or null), yaw°, pitch°, held actions, seconds, screenshot at the end]
const ENEMY_MOVES := [
	["watch it wake up", Vector3(0, 0.05, 10), 0, 0, [], 1.5, true],
	["watch it come across the hall", null, 0, 0, [], 5.0, true],
	["go into the mine", Vector3(-1, 0.05, 32), 0, 0, [], 0.5, false],
	["watch it follow into the mine", null, 0, 0, [], 9.0, true],
	["up on the platform", Vector3(-11, 2.05, -10), 0, 0, [], 0.5, false],
	["watch it find the stairs", null, 0, 0, [], 6.0, true],
	["watch it climb", null, 0, 0, [], 6.0, true],
	["watch: shoot it", null, 0, 0, ["fire"], 0.03, true],
	["watch it get knocked", null, 0, 0, [], 0.6, true],
]

var enemy: Node3D
var body: RigidBody3D
var debug := PathDebug.new()
var _last_target := Vector3.INF


func _ready() -> void:
	super()
	_lamps_lit = true
	level.set_lamps(true)
	NavBake.add_to(level)
	enemy = _enemy_scene().instantiate()
	enemy.get_node("Body").position = ENEMY_SPAWN
	add_child(enemy)
	body = enemy.get_node("Body")
	debug.body = body
	add_child(debug)


func _enemy_scene() -> PackedScene:
	return ENEMY


func _physics_process(_delta: float) -> void:
	if NavigationServer3D.map_get_iteration_id(get_world_3d().navigation_map) == 0:
		return  # the navmesh isn't in the map yet
	var at := player.global_position
	var lost: bool = body.target_reached() and body.target.distance_to(at) > RETARGET  # no route found
	if at.distance_to(_last_target) > RETARGET or (lost and Engine.get_physics_frames() % 30 == 0):
		_last_target = at
		body.set_target(at)


func _process(delta: float) -> void:
	super(delta)
	var moves := _moves()
	if _auto and _move >= 0 and _move < moves.size() and moves[_move][0].begins_with("watch"):
		_look_at_enemy()


## Autopilot camera: keep the ball in the middle of the screen.
func _look_at_enemy() -> void:
	var head: Node3D = player.get_node("Head")
	var to := body.global_position - head.global_position
	var flat := Vector2(to.x, to.z).length()
	head.face(atan2(-to.x, -to.z), atan2(to.y, flat))


func _unhandled_input(event: InputEvent) -> void:
	super(event)
	if event is InputEventKey and event.pressed and event.physical_keycode == KEY_P:
		debug.visible = not debug.visible


func _moves() -> Array:
	return ENEMY_MOVES


func _title() -> String:
	return "Step 6 · enemy body   (P route · L lamps · F flashlight · LMB fire · RMB aim · R reload · WASD)"


func _status_extra() -> String:
	if body == null:
		return super()
	return "%s\nenemy: %s · %.1f m/s · route %d · %.1f m from you" % [super(), GROUNDED[body.grounded],
		body.linear_velocity.length(), body.route.points.size(), body.global_position.distance_to(player.global_position)]


func _shot_prefix() -> String:
	return "step06"
