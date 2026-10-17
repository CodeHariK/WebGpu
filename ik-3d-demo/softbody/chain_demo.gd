## Softbody step 1b: a Kratos-style chain. One VerletBody holds everything: the hand, the chain
## links (light), the weight on the end (heavy), the crates (each its own mass) and the post (pinned).
##
## The chain is slack sticks (they only pull). Its length is one number: throwing lets it out to
## full length, pulling reels it in by shortening every link a little each step — the sticks then
## drag whatever is on the end toward the hand. In flight the weight counts as very heavy, so the
## chain pays out behind it instead of slowing it down (a real chain slides out of the hand). The hook is just one more stick, added on a hit
## (weight ↔ target) and removed on let-go.
##   Crate: the hand is planted (pinned), so the crate comes to you; heavier crates reel slower.
##   Post:  the post is pinned, so the hand is let go instead and you are pulled to it (grapple).
##
##   WASD move    Left click throw at the mouse (aim near a crate / the post to hook it)
##   Hold right mouse  pull    Space  let go    R  reset
extends Node3D

enum State { IDLE, FLYING, ATTACHED, RECALL }

const LINKS := 40
const MAX_LENGTH := 14.0
const IDLE_LENGTH := 0.75 ## weight hanging from the hand
const LINK_MASS := 0.15
const WEIGHT_MASS := 4.0
const FLYING_MASS := 1000.0 ## in flight the chain can't slow the weight: it pays out behind it
const WEIGHT_RADIUS := 0.22
const HAND_HEIGHT := 1.1 ## the hand point stays this far above the floor while walking
const PLAYER_SPEED := 4.0
const THROW_SPEED := 22.0
const FLY_TIME := 0.7 ## no hit by then: recall
const REEL_SPEED := 7.0 ## m/s of chain pulled in (÷ √mass for crates)
const RECALL_SPEED := 18.0
const ARRIVED := 1.3 ## hand ↔ target distance (plus its half size) at which a pull lets go (it keeps its speed)
const MAX_TENSION := 0.8 ## reel at most this far below the chain's real length: a pull, not a teleport
const AIM_PIXELS := 70.0 ## aim snaps to a target this close to the mouse on screen
const CAMERA_OFFSET := Vector3(0, 7.0, 9.0)

## name, mass (kg), box size (m), position, fixed (post)
const TARGETS := [
	["1 kg", 1.0, 0.45, Vector3(-4, 0, -5), false],
	["4 kg", 4.0, 0.8, Vector3(0, 0, -7), false],
	["15 kg", 15.0, 1.2, Vector3(4.5, 0, -5), false],
	["post (grapple)", 0.0, 0.35, Vector3(-1, 2.6, -11), true],
]

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var _body: VerletBody
var _chain: LimbChain
var _renderer: LimbRenderer
var _hand := 0 ## point index
var _weight := LINKS ## point index (the last chain point)
var _targets: Array[Dictionary] = [] ## {point, mass, size, fixed, mesh}
var _player_mesh: MeshInstance3D
var _weight_mesh: MeshInstance3D

var _state := State.IDLE
var _length := IDLE_LENGTH
var _timer := 0.0
var _target := -1 ## index into _targets while hooked
var _hook_stick := -1
var _pulling := false
var _grappling := false ## the hand is free (being pulled to the post, or falling after)


func _ready() -> void:
	_add_ground()
	_renderer = LimbRenderer.new()
	_renderer.name = "LimbRenderer"
	add_child(_renderer)
	_player_mesh = _mesh_instance(_capsule(), Color(0.85, 0.35, 0.3))
	_weight_mesh = _mesh_instance(_sphere(WEIGHT_RADIUS), Color(0.25, 0.25, 0.28))
	_build()


func _build() -> void:
	for target in _targets:
		target.mesh.queue_free()
	_targets.clear()
	_body = VerletBody.new()
	_body.iterations = 40 # a fix travels about one link per pass: 40 links need ~40 to stay stiff
	_body.damping = 0.995
	_body.floor_friction = 0.15 # per step: crates slide a little when pulled
	var hand_position := Vector3(0, HAND_HEIGHT, 0)
	_hand = _body.add_point(hand_position, true, 6.0, HAND_HEIGHT)
	for i in range(1, LINKS + 1): # links, the weight last
		var is_weight := i == LINKS
		_body.add_point(hand_position + Vector3(0, -IDLE_LENGTH * i / LINKS, 0), false,
				WEIGHT_MASS if is_weight else LINK_MASS, WEIGHT_RADIUS if is_weight else 0.02)
		_body.add_stick(i - 1, i, true)
	_weight = LINKS
	for spec in TARGETS:
		var size: float = spec[2]
		var fixed: bool = spec[4]
		var position: Vector3 = spec[3] + (Vector3.ZERO if fixed else Vector3(0, size * 0.5, 0))
		var point := _body.add_point(position, fixed, maxf(spec[1], 0.1), size * 0.5)
		var mesh := _mesh_instance(_post(position.y) if fixed else _box(size),
				Color(0.5, 0.5, 0.55) if fixed else Color(0.75, 0.55, 0.3))
		var label := Label3D.new()
		label.text = spec[0]
		label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label.pixel_size = 0.008
		label.font_size = 48
		label.position = Vector3(0, size * 0.5 + 0.35, 0)
		mesh.add_child(label)
		_targets.append({"point": point, "mass": spec[1], "size": size, "fixed": fixed, "mesh": mesh})
	var lengths := PackedFloat32Array()
	lengths.resize(LINKS)
	lengths.fill(IDLE_LENGTH / LINKS)
	_renderer.clear()
	_chain = LimbChain.new(lengths, hand_position, LimbChain.Solver.FK)
	_chain.points = _body.points.slice(0, LINKS + 1)
	_chain.radius = 0.02
	_chain.joint_radius = 0.035
	_chain.color = Color(0.6, 0.6, 0.65)
	_renderer.add(_chain)
	_state = State.IDLE
	_length = IDLE_LENGTH
	_target = -1
	_hook_stick = -1
	_grappling = false


func _physics_process(delta: float) -> void:
	_timer += delta
	_move_player(delta)
	_update_length(delta)
	for s in LINKS: # the chain's length, shared by every link
		_body.rest[s] = _length / LINKS
	_body.mass[_weight] = FLYING_MASS if _state == State.FLYING else WEIGHT_MASS
	_body.step(delta)
	if _state == State.FLYING:
		_check_hit()
		if _timer > FLY_TIME:
			_state = State.RECALL


# Walking: the hand is planted (pinned) and moved by hand. Grappling: it's a free heavy point
# pulled by the chain and gravity, until it lands.
func _move_player(delta: float) -> void:
	if _grappling:
		var landed := _body.points[_hand].y <= HAND_HEIGHT + 0.01 and _body.velocity(_hand, delta).y <= 0.1
		if landed and _state != State.ATTACHED:
			_grappling = false
	_body.pinned[_hand] = 0 if _grappling else 1
	if _grappling:
		return
	var input := Vector2(
			Input.get_action_strength("ui_right") + float(Input.is_key_pressed(KEY_D)) - Input.get_action_strength("ui_left") - float(Input.is_key_pressed(KEY_A)),
			Input.get_action_strength("ui_down") + float(Input.is_key_pressed(KEY_S)) - Input.get_action_strength("ui_up") - float(Input.is_key_pressed(KEY_W)))
	var step := Vector3(input.x, 0, input.y).limit_length(1.0) * PLAYER_SPEED * delta
	var hand := _body.points[_hand] + step
	hand.y = HAND_HEIGHT
	_body.set_point(_hand, hand)


# The chain length per state: out to full on a throw, held when hooked, reeled while pulling.
func _update_length(delta: float) -> void:
	match _state:
		State.IDLE:
			_length = move_toward(_length, IDLE_LENGTH, RECALL_SPEED * delta)
		State.RECALL:
			_length = move_toward(_length, IDLE_LENGTH, RECALL_SPEED * delta)
			if _length <= IDLE_LENGTH:
				_state = State.IDLE
		State.ATTACHED:
			if not _pulling:
				return
			var target: Dictionary = _targets[_target]
			var speed: float = REEL_SPEED * 1.5 if target.fixed else REEL_SPEED / sqrt(maxf(target.mass, 1.0))
			if target.fixed:
				_grappling = true # the post can't move, so you do
			_length = maxf(_length - speed * delta, _chain_length() - MAX_TENSION)
			var gap: float = _body.points[_hand].distance_to(_body.points[target.point]) - target.size * 0.5
			if gap <= ARRIVED:
				_let_go()


func _throw(mouse: Vector2) -> void:
	if _state != State.IDLE:
		return
	var aim := _aim_point(mouse)
	var from := _body.points[_weight]
	var flat_distance := Vector2(aim.x - from.x, aim.z - from.z).length()
	var velocity := (aim - from).normalized() * THROW_SPEED
	velocity.y += 0.5 * 9.8 * flat_distance / THROW_SPEED # lift so the arc lands on the aim point
	_body.set_velocity(_weight, velocity, get_physics_process_delta_time())
	_length = MAX_LENGTH # let it all out: slack sticks don't hold the weight back until taut
	_state = State.FLYING
	_timer = 0.0


# A target near the mouse on screen, else the floor under the mouse (at waist height).
func _aim_point(mouse: Vector2) -> Vector3:
	var best := AIM_PIXELS
	var aim = null
	for target in _targets:
		var p := _body.points[target.point]
		var distance := camera.unproject_position(p).distance_to(mouse)
		if distance < best:
			best = distance
			aim = p
	if aim != null:
		return aim
	var hit = Plane(Vector3.UP, 0.8).intersects_ray(camera.project_ray_origin(mouse), camera.project_ray_normal(mouse))
	return hit if hit != null else _body.points[_hand] + Vector3(0, 0, -5)


func _check_hit() -> void:
	for t in _targets.size():
		var target: Dictionary = _targets[t]
		var reach: float = WEIGHT_RADIUS + target.size * 0.5 + 0.1
		if _body.points[_weight].distance_to(_body.points[target.point]) > reach:
			continue
		_hook_stick = _body.add_stick(_weight, target.point) # the hook: one more stick
		_body.rest[_hook_stick] = reach - 0.1
		_target = t
		_length = minf(_chain_length(), MAX_LENGTH)
		_state = State.ATTACHED
		return


func _let_go() -> void:
	if _hook_stick >= 0:
		_body.remove_stick(_hook_stick)
	_hook_stick = -1
	_target = -1
	if _state != State.IDLE:
		_state = State.RECALL


func _chain_length() -> float:
	var total := 0.0
	for i in LINKS:
		total += _body.points[i].distance_to(_body.points[i + 1])
	return total


func _process(delta: float) -> void:
	var hand := _body.points[_hand]
	_player_mesh.position = hand + Vector3(0, -0.3, 0)
	_weight_mesh.position = _body.points[_weight]
	for target in _targets:
		var p := _body.points[target.point]
		target.mesh.position = p if not target.fixed else Vector3(p.x, 0, p.z)
	_chain.points = _body.points.slice(0, LINKS + 1)
	_renderer.draw()
	var focus := Vector3(hand.x, 0, hand.z)
	camera.position = camera.position.lerp(focus + CAMERA_OFFSET, 1.0 - exp(-4.0 * delta))
	camera.look_at(camera.position - CAMERA_OFFSET + Vector3(0, 0.5, 0))
	var hooked := "" if _target < 0 else "  hooked: %s" % TARGETS[_target][0]
	hud.text = "%s  chain %.1f m%s%s\nWASD move   Left click throw (aim at a crate / the post)   Hold right mouse pull   Space let go   R reset" % [
		State.keys()[_state], _length, hooked, "  (grappling)" if _grappling else ""]


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			_throw(event.position)
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			_pulling = event.pressed
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_SPACE:
				_let_go()
			KEY_R:
				_build()


# --- looks ---------------------------------------------------------------------------------------

func _mesh_instance(mesh: Mesh, color: Color) -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.6
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	add_child(instance)
	return instance


func _capsule() -> Mesh:
	var mesh := CapsuleMesh.new()
	mesh.radius = 0.3
	mesh.height = 1.6
	return mesh


func _sphere(r: float) -> Mesh:
	var mesh := SphereMesh.new()
	mesh.radius = r
	mesh.height = r * 2.0
	return mesh


func _box(size: float) -> Mesh:
	var mesh := BoxMesh.new()
	mesh.size = Vector3.ONE * size
	return mesh


func _post(height: float) -> Mesh: # a pole from the floor up to the hook point (mesh origin at the floor)
	var mesh := CylinderMesh.new()
	mesh.top_radius = 0.15
	mesh.bottom_radius = 0.2
	mesh.height = height + 0.2
	var offset := ArrayMesh.new()
	var arrays := mesh.get_mesh_arrays()
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	for i in vertices.size():
		vertices[i].y += (height + 0.2) * 0.5
	arrays[Mesh.ARRAY_VERTEX] = vertices
	offset.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return offset


func _add_ground() -> void:
	var mesh := PlaneMesh.new()
	mesh.size = Vector2(60, 60)
	var ground := MeshInstance3D.new()
	ground.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.4, 0.45, 0.4)
	ground.material_override = material
	add_child(ground)
