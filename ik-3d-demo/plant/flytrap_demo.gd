## Demo for the dancing Venus flytrap. The glowing orb is the prey: it drifts round the plant,
## swinging in and out of reach, so the heads watch it while they dance and snap when it comes
## close.
##   Space  dance on/off       S  snap now       ← →  tempo slower / faster
##   H      heads: 1 → 3 → 5   O  orb drifting / parked out of reach
extends Node3D

const ORB_HEIGHT := 1.0
const ORB_RADIUS_RANGE := Vector2(1.2, 3.8) ## the orb's distance from the plant swings between these

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var flytrap: Flytrap
var orb: Node3D
var orb_moving := true
var _time := 0.0


func _ready() -> void:
	_add_ground()
	orb = _add_orb()
	flytrap = Flytrap.new()
	flytrap.name = "Flytrap"
	flytrap.target = orb
	add_child(flytrap)
	camera.look_at(Vector3(0, 0.9, 0))


func _process(delta: float) -> void:
	_time += delta
	if orb_moving: # round the plant at a slowly breathing distance: comes in close, backs off
		var distance := lerpf(ORB_RADIUS_RANGE.x, ORB_RADIUS_RANGE.y, 0.5 + 0.5 * sin(_time * 0.45))
		var angle := _time * 0.5
		orb.position = Vector3(sin(angle) * distance, ORB_HEIGHT + sin(_time * 1.7) * 0.15, cos(angle) * distance)
	else:
		orb.position = Vector3(0, ORB_HEIGHT, ORB_RADIUS_RANGE.y)
	var states := []
	for stalk in flytrap.stalks:
		states.append(stalk.state)
	hud.text = "Space dance %s   S snap   ← → tempo %.2f beats/s   H heads %d   O orb %s\nheads: %s" % [
		"ON" if flytrap.dance else "off", flytrap.tempo, flytrap.heads,
		"drifting" if orb_moving else "parked", ", ".join(states),
	]


func _unhandled_key_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	match key.keycode:
		KEY_SPACE: flytrap.dance = not flytrap.dance
		KEY_S: flytrap.snap_all()
		KEY_LEFT: flytrap.tempo = maxf(flytrap.tempo - 0.2, 0.2)
		KEY_RIGHT: flytrap.tempo = minf(flytrap.tempo + 0.2, 4.0)
		KEY_H: flytrap.heads = {1: 3, 3: 5}.get(flytrap.heads, 1)
		KEY_O: orb_moving = not orb_moving


func _add_ground() -> void:
	var shape := BoxShape3D.new()
	shape.size = Vector3(30, 1, 30)
	var mesh := BoxMesh.new()
	mesh.size = shape.size
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.42, 0.55, 0.38)
	var ground := StaticBody3D.new()
	ground.position = Vector3(0, -0.5, 0)
	var collision := CollisionShape3D.new()
	collision.shape = shape
	ground.add_child(collision)
	var visual := MeshInstance3D.new()
	visual.mesh = mesh
	visual.material_override = material
	ground.add_child(visual)
	add_child(ground)


func _add_orb() -> Node3D:
	var sphere := SphereMesh.new()
	sphere.radius = 0.15
	sphere.height = 0.3
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.6, 1.0, 0.7)
	material.emission_enabled = true
	material.emission = Color(0.4, 1.0, 0.6)
	material.emission_energy_multiplier = 2.0
	var ball := MeshInstance3D.new()
	ball.name = "Prey"
	ball.mesh = sphere
	ball.material_override = material
	add_child(ball)
	return ball
