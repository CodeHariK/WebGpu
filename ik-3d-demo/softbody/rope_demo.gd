## Softbody step 1: rope. Three ropes made of the same points + sticks (VerletBody), each fixing
## its sticks a different number of times per step — the only difference between a stretchy bungee
## and a stiff chain. Drawn with LimbRenderer (one LimbChain per rope, its points copied in).
##
##   Drag (left mouse)  grab any point; let go while moving to throw it
##   Space  drop / re-pin the ropes' tops      D  damping (how fast swinging dies out)
##   G      gravity on / off                   R  reset
extends Node3D

const SEGMENTS := 24
const SEGMENT_LENGTH := 0.1
const TOP_HEIGHT := 3.0
const ROPE_ITERATIONS := [1, 6, 40] ## left: bungee, middle: rope, right: chain
const ROPE_COLORS := [Color(1.0, 0.55, 0.3), Color(0.95, 0.85, 0.4), Color(0.55, 0.8, 1.0)]
const ROPE_SPACING := 1.6
const RADIUS := 0.03
const DAMPING_STEPS := [0.9, 0.97, 0.99, 0.999]
const GRAB_PIXELS := 40.0 ## how close (on screen) the mouse must be to grab a point

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var _bodies: Array[VerletBody] = []
var _chains: Array[LimbChain] = []
var _renderer: LimbRenderer
var _damping_index := 2
var _gravity_on := true
var _tops_pinned := true

# Grab: which body and point, the plane it moves on (faces the camera), and where the mouse aims.
var _grab_body := -1
var _grab_point := -1
var _grab_was_pinned := false
var _grab_plane := Plane()
var _grab_target := Vector3.ZERO


func _ready() -> void:
	_add_ground()
	_renderer = LimbRenderer.new()
	_renderer.name = "LimbRenderer"
	add_child(_renderer)
	_build()


# One VerletBody per rope: a column of points, each joined to the next by a stick; the top pinned.
# The rope starts sideways (horizontal) so it swings down on the first frames.
func _build() -> void:
	_bodies.clear()
	_chains.clear()
	_renderer.clear()
	for r in ROPE_ITERATIONS.size():
		var top := Vector3((r - 1) * ROPE_SPACING, TOP_HEIGHT, 0.0)
		var body := VerletBody.new()
		body.iterations = ROPE_ITERATIONS[r]
		body.floor_height = RADIUS
		for i in SEGMENTS + 1:
			body.add_point(top + Vector3(i * SEGMENT_LENGTH * 0.7, -i * SEGMENT_LENGTH * 0.7, 0.0), i == 0)
		for i in SEGMENTS:
			body.add_stick(i, i + 1)
		_bodies.append(body)
		var lengths := PackedFloat32Array()
		lengths.resize(SEGMENTS)
		lengths.fill(SEGMENT_LENGTH)
		var chain := LimbChain.new(lengths, top, LimbChain.Solver.FK) # FK: we write its points
		chain.radius = RADIUS
		chain.joint_radius = RADIUS
		chain.color = ROPE_COLORS[r]
		_chains.append(chain)
		_renderer.add(chain)
		_add_label(top + Vector3(0, 0.35, 0), "%d pass%s" % [ROPE_ITERATIONS[r], "" if ROPE_ITERATIONS[r] == 1 else "es"])
	_apply_settings()


func _apply_settings() -> void:
	for body in _bodies:
		body.damping = DAMPING_STEPS[_damping_index]
		body.gravity = Vector3(0, -9.8, 0) if _gravity_on else Vector3.ZERO
		body.pinned[0] = 1 if _tops_pinned else 0


# Fixed 60 steps a second: the same motion at any frame rate.
func _physics_process(delta: float) -> void:
	if _grab_body >= 0:
		_bodies[_grab_body].set_point(_grab_point, _grab_target)
	for body in _bodies:
		body.step(delta)


func _process(_delta: float) -> void:
	for r in _bodies.size():
		_chains[r].points = _bodies[r].points
	_renderer.draw()
	var stretch := ""
	for r in _bodies.size():
		stretch += "  %d pass: %+.1f%%" % [ROPE_ITERATIONS[r], _bodies[r].stretch() * 100.0]
	hud.text = "stretch%s     damping %.3f   gravity %s   tops %s\nDrag a point (throw it)   Space drop/pin tops   D damping   G gravity   R reset" % [
		stretch, DAMPING_STEPS[_damping_index], "on" if _gravity_on else "off", "pinned" if _tops_pinned else "free"]


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed:
			_grab(event.position)
		else:
			_release()
	elif event is InputEventMouseMotion and _grab_body >= 0:
		_aim(event.position)
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_SPACE:
				_tops_pinned = not _tops_pinned
				if _tops_pinned: # put the tops back where they hang from
					for r in _bodies.size():
						_bodies[r].points[0] = Vector3((r - 1) * ROPE_SPACING, TOP_HEIGHT, 0.0)
						_bodies[r].previous[0] = _bodies[r].points[0]
				_apply_settings()
			KEY_D:
				_damping_index = (_damping_index + 1) % DAMPING_STEPS.size()
				_apply_settings()
			KEY_G:
				_gravity_on = not _gravity_on
				_apply_settings()
			KEY_R:
				_release()
				for child in get_children():
					if child is Label3D:
						child.queue_free()
				_build()


# Grab the point nearest the mouse on screen (if close enough). While held it is pinned and
# follows the mouse on a plane through it that faces the camera.
func _grab(mouse: Vector2) -> void:
	var best := GRAB_PIXELS
	for r in _bodies.size():
		var points := _bodies[r].points
		for i in points.size():
			if camera.is_position_behind(points[i]):
				continue
			var distance := camera.unproject_position(points[i]).distance_to(mouse)
			if distance < best:
				best = distance
				_grab_body = r
				_grab_point = i
	if _grab_body < 0:
		return
	var body := _bodies[_grab_body]
	_grab_was_pinned = body.pinned[_grab_point] == 1
	body.pinned[_grab_point] = 1
	_grab_target = body.points[_grab_point]
	_grab_plane = Plane(-camera.global_basis.z, _grab_target)


func _aim(mouse: Vector2) -> void:
	var hit = _grab_plane.intersects_ray(camera.project_ray_origin(mouse), camera.project_ray_normal(mouse))
	if hit != null:
		_grab_target = hit


# Let go: the point keeps the speed it was dragged at (Verlet remembers last step's position).
func _release() -> void:
	if _grab_body >= 0 and _grab_body < _bodies.size():
		_bodies[_grab_body].pinned[_grab_point] = 1 if _grab_was_pinned else 0
	_grab_body = -1
	_grab_point = -1


func _add_label(position: Vector3, text: String) -> void:
	var label := Label3D.new()
	label.text = text
	label.position = position
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.font_size = 48
	label.pixel_size = 0.004
	add_child(label)


func _add_ground() -> void:
	var mesh := PlaneMesh.new()
	mesh.size = Vector2(40, 40)
	var ground := MeshInstance3D.new()
	ground.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.4, 0.45, 0.4)
	ground.material_override = material
	add_child(ground)
