## Demo for the bats: one detailed IK bat (Bat) and a cheap MultiMesh swarm (BatSwarm). The
## glowing orb is the target: it wanders round the clearing, in and out of reach of the tree the
## bat roosts on.
##   1 2 3 4  bat: fly / hover / swoop / roost      A  bat picks states by itself on/off
##   W  swarm on/off     E  swarm circle ↔ attack    Q  scatter the swarm    [ ]  swarm −/+ 20
##   O  orb wandering / parked      C  camera: wide ↔ follow the bat
extends Node3D

const ORB_HEIGHT := 1.0
const TREE_AT := Vector3(-2.6, 0, -1.6)
const BRANCH_HEIGHT := 2.7
const WIDE_CAMERA := Vector3(0.5, 2.8, 6.8)

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var bat: Bat
var swarm: BatSwarm
var orb: Node3D
var perch: Marker3D
var orb_moving := true
var follow := false
var _time := 0.0


func _ready() -> void:
	_add_ground()
	_add_tree()
	orb = _add_orb()
	bat = Bat.new()
	bat.name = "Bat"
	bat.target = orb
	bat.perch = perch
	bat.position = Vector3(1.5, 2.0, 0.5)
	add_child(bat)
	swarm = BatSwarm.new()
	swarm.name = "BatSwarm"
	swarm.target = orb
	add_child(swarm)
	camera.position = WIDE_CAMERA
	camera.look_at(Vector3(0, 1.5, 0))


func _process(delta: float) -> void:
	_time += delta
	if orb_moving: # a slow figure-8 that passes under the roost now and then
		orb.position = Vector3(sin(_time * 0.35) * 2.4, ORB_HEIGHT + sin(_time * 1.3) * 0.15, sin(_time * 0.7) * 1.6)
	else:
		orb.position = Vector3(1.5, ORB_HEIGHT, 1.0)
	if follow:
		var behind := bat.global_position + Vector3(0, 0.6, 2.2)
		camera.position = camera.position.lerp(behind, 1.0 - exp(-3.0 * delta))
		camera.look_at(bat.global_position)
	hud.text = "bat: %s%s   auto %s   |   swarm %s: %s, %d bats   |   orb %s   %d fps\n%s" % [
		bat.state, " · " + bat.phase if bat.phase != "" else "", "ON" if bat.auto else "off",
		"ON" if swarm.visible else "off", swarm.mode, swarm.count,
		"wandering" if orb_moving else "parked", Engine.get_frames_per_second(),
		"1 fly  2 hover  3 swoop  4 roost  A auto   W swarm  E attack  Q scatter  [ ] count   O orb  C camera",
	]


func _unhandled_key_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	match key.keycode:
		KEY_1: bat.go("fly")
		KEY_2: bat.go("hover")
		KEY_3: bat.go("swoop")
		KEY_4: bat.go("roost")
		KEY_A: bat.auto = not bat.auto
		KEY_W: _toggle_swarm()
		KEY_E: swarm.mode = "attack" if swarm.mode == "circle" else "circle"
		KEY_Q: swarm.scatter()
		KEY_BRACKETLEFT: swarm.count = maxi(swarm.count - 20, 1)
		KEY_BRACKETRIGHT: swarm.count = mini(swarm.count + 20, 400)
		KEY_O: orb_moving = not orb_moving
		KEY_C: _toggle_camera()


func _toggle_swarm() -> void:
	swarm.visible = not swarm.visible
	swarm.process_mode = Node.PROCESS_MODE_INHERIT if swarm.visible else Node.PROCESS_MODE_DISABLED


func _toggle_camera() -> void:
	follow = not follow
	if not follow:
		camera.position = WIDE_CAMERA
		camera.look_at(Vector3(0, 1.5, 0))


func _add_ground() -> void:
	var shape := BoxShape3D.new()
	shape.size = Vector3(30, 1, 30)
	var ground := StaticBody3D.new()
	ground.position = Vector3(0, -0.5, 0)
	var collision := CollisionShape3D.new()
	collision.shape = shape
	ground.add_child(collision)
	var mesh := BoxMesh.new()
	mesh.size = shape.size
	ground.add_child(BatMeshes.instance(mesh, Transform3D.IDENTITY, Color(0.3, 0.38, 0.32)))
	add_child(ground)


# A trunk with one branch sticking out; the bat hangs under the branch's tip.
func _add_tree() -> void:
	var bark := Color(0.4, 0.3, 0.22)
	var trunk := CylinderMesh.new()
	trunk.top_radius = 0.14
	trunk.bottom_radius = 0.22
	trunk.height = 3.4
	add_child(BatMeshes.instance(trunk, Transform3D(Basis.IDENTITY, TREE_AT + Vector3(0, 1.7, 0)), bark))
	var branch := CylinderMesh.new()
	branch.top_radius = 0.04
	branch.bottom_radius = 0.07
	branch.height = 1.3
	var along := Vector3(1, 0.12, 0.35).normalized()
	var start := TREE_AT + Vector3(0, BRANCH_HEIGHT, 0)
	add_child(BatMeshes.instance(branch, Transform3D(BatMeshes.basis_along(along), start + along * 0.65), bark))
	add_child(BatMeshes.blob(Vector3(0.7, 0.45, 0.7), TREE_AT + Vector3(0, 3.5, 0), Color(0.25, 0.42, 0.28)))
	perch = Marker3D.new()
	perch.name = "Perch"
	perch.position = start + along * 1.05 - Vector3(0, 0.05, 0) # underside of the branch near its tip
	add_child(perch)


func _add_orb() -> Node3D:
	var ball := BatMeshes.instance(BatMeshes.sphere(0.15), Transform3D.IDENTITY, Color(0.6, 1.0, 0.7), 2.0)
	ball.name = "Orb"
	add_child(ball)
	return ball
