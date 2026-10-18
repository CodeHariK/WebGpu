## A target that gets hit, knocked, yanked, bound and thrown around by the blades (or hammers).
## It falls with gravity and slides to a stop on the ground; moves that carry it (yank, whirl)
## set `yanked` and place it themselves, Chain Bind sets `bound` and it stays put.
##
## It shows as a boxy robot (body, head, red eyes — the hammer demo's robots) or as the old capsule
## dummy (`robots`, N switches). The robot also tumbles like a thrown crate: knocked into the air it
## spins the way it was hit; when a corner of its box meets the ground it bounces (losing most of
## its fall), skids and loses spin; once it has nearly stopped it settles — upright if it was
## close, else onto its side, where it lies a moment and then hauls itself back up. A slam (squash)
## drives its head nearly flat into its shoulders and buckles its body; they shudder back up over
## about a second (the hammer demo's crush). Robots come in different heights (`height`, the top of
## the head), so a jump smash has to find each one's head. The one a click would go for glows
## (`highlight`). Hit, its eyes flicker for a while (FLICKER_TIME), like
## something knocked loose inside.
class_name DemoEnemy
extends RefCounted

const GRAVITY := 20.0
const STAND_HEIGHT := 1.0 ## a 2 m robot's centre when standing (each one's own: `stand`)
const FRICTION := 8.0 ## how fast a slide stops on the ground
const BODY := Vector2(0.85, 0.6) ## the robot's body: width, depth (its height: the rest under the head)
const HEAD := 0.6 ## its head: a cube on top
const SPIN := 0.9 ## radians per second of tumble for each m/s it's knocked across the ground
const LIE := 0.8 ## seconds it lies there after landing tipped over …
const GET_UP := 0.5 ## … and takes to get up
const BOUNCE := 0.3 ## the share of its falling speed a bounce keeps …
const SKID := 0.6 ## … and of its sliding speed and spin
const FLICKER_TIME := 1.6 ## seconds its eyes flicker after a hit
const HALF := Vector2(0.43, 0.3) ## half its box across (and half its height: `stand`); it turns about its middle (`position`)

static var robots := true ## show robots (else the capsule dummies)

var position := Vector3.ZERO
var velocity := Vector3.ZERO
var flash := 0.0 ## 1 on a hit, fades out (the red glow)
var yanked := false ## a move is carrying it
var bound := 0.0 ## seconds left held by Chain Bind (can't move)
var thrown := false ## flying after a Whirl: knocks over others until it lands
var cooldown := 0.0 ## can't be body-hit again for this long
var squash := 0.0 ## 1 when smashed into the ground (a slam), springs back to 0: flattens it
var height := 2.0 ## m: the top of its head above the ground
var stand := STAND_HEIGHT ## its centre above the ground, standing (half its height)
var highlight := 0.0 ## 1: a click would go for it (it glows a little)
var mesh: Node3D ## everything it shows (the robot and the dummy capsule under one node)
var material: StandardMaterial3D

var _dummy: MeshInstance3D
var _robot := Node3D.new()
var _body: MeshInstance3D
var _head: MeshInstance3D
var _robot_material: StandardMaterial3D
var _owner: Node3D
var _tumble := Basis.IDENTITY ## how it's turned over (on top of facing you)
var _spin := Vector3.ZERO ## its tumble (axis × radians per second)
var _down := 0.0 ## seconds left lying there (then it gets up)
var _rise := 1.0 ## getting up: 0 lying … 1 standing
var _lying := Basis.IDENTITY ## the tumble it lies in
var _facing := Basis.IDENTITY ## which way it faces (toward you; held still while it tumbles)
var _eyes: Array[MeshInstance3D] = []
var _hurt := -1.0 ## seconds since it was last hit (−1: not lately): its eyes flicker
var _last_flash := 0.0
var _crushed := -1.0 ## seconds since a slam crushed it (−1: not lately)
var _crush := 0.0 ## how hard (the squash it was given)
var _last_squash := 0.0


func _init(
		parent: Node3D,
		ground_position: Vector3,
		new_height := 2.0
) -> void:
	_owner = parent
	height = new_height
	stand = height * 0.5
	position = ground_position + Vector3.UP * stand
	mesh = Node3D.new()
	parent.add_child(mesh)
	material = StandardMaterial3D.new()
	material.albedo_color = Color(0.45, 0.55, 0.75)
	_dummy = MeshInstance3D.new()
	_dummy.mesh = DemoProps.capsule(0.4, height)
	_dummy.material_override = material
	mesh.add_child(_dummy)
	_build_robot()


## The top of its head (where a smash lands), where it is now.
func head_top() -> Vector3:
	return position + Vector3.UP * stand


func update(delta: float) -> void:
	if flash > _last_flash + 0.01: # a fresh hit (every hit lights it up)
		_hurt = 0.0
	if _hurt >= 0.0:
		_hurt += delta
		if _hurt > FLICKER_TIME:
			_hurt = -1.0
	flash = maxf(flash - delta * 4.0, 0.0)
	_last_flash = flash
	if squash > _last_squash + 0.01: # a fresh slam: the crush starts
		_crushed = 0.0
		_crush = squash
	if _crushed >= 0.0:
		_crushed += delta
		if _crushed > 2.0:
			_crushed = -1.0
	squash = maxf(squash - delta * 3.0, 0.0)
	_last_squash = squash
	cooldown = maxf(cooldown - delta, 0.0)
	if yanked:
		_spin = Vector3.ZERO
		_down = 0.0
		_rise = 1.0
		_tumble = _tumble.slerp(Basis.IDENTITY, 1.0 - exp(-10.0 * delta)) # carried: held upright
		return
	if bound > 0.0:
		bound -= delta
		velocity = Vector3.ZERO
		return
	if robots:
		_tumble_robot(delta)
		return
	var airborne := position.y > stand + 0.02 or velocity.y > 0.0
	if airborne and _spin == Vector3.ZERO: # just knocked up: it tumbles the way it was hit
		var across := Vector3(velocity.x, 0.0, velocity.z)
		if across.length() > 0.5:
			_spin = Vector3.UP.cross(across.normalized()) * across.length() * SPIN
	velocity.y -= GRAVITY * delta
	position += velocity * delta
	if airborne and _spin != Vector3.ZERO:
		_tumble = Basis(_spin.normalized(), _spin.length() * delta) * _tumble
	if position.y <= stand:
		position.y = stand
		if airborne:
			_land()
		thrown = false
		var slow := exp(-FRICTION * delta)
		velocity = Vector3(velocity.x * slow, 0.0, velocity.z * slow)
	_get_up(delta)


func draw() -> void:
	_dummy.visible = not robots
	_robot.visible = robots
	var flat := squash * squash # quick to spring back at the end
	if not robots:
		_dummy.scale = Vector3(1.0 + flat * 0.35, 1.0 - flat * 0.5, 1.0 + flat * 0.35)
		mesh.transform = Transform3D(Basis.IDENTITY, position - Vector3.UP * (stand * flat * 0.5)) # squashed onto the ground, not into the air
		material.emission_enabled = flash > 0.0 or highlight > 0.0
		material.emission = Color(1, 0.3, 0.2) * flash + Color(1.0, 0.55, 0.15) * highlight * 0.35
		return
	_draw_robot(flat)


# The robot's flight: gravity, its spin, and its box's corners against the ground (bounce, skid);
# standing still on its feet, it just slides to a stop like the dummy.
func _tumble_robot(delta: float) -> void:
	var settled := _down <= 0.0 and _rise >= 1.0 and _spin == Vector3.ZERO
	if settled and position.y <= stand + 0.02 and velocity.y <= 0.0: # standing: slide to a stop
		position.y = stand
		thrown = false
		var slow := exp(-FRICTION * delta)
		velocity = Vector3(velocity.x * slow, 0.0, velocity.z * slow)
		return
	if _down > 0.0 or _rise < 1.0: # lying there, or getting up
		velocity = Vector3.ZERO
		_get_up(delta)
		var lie_height := lerpf(_lowest_rest(_lying), stand, _rise * _rise * (3.0 - 2.0 * _rise))
		position.y = lie_height
		return
	if _spin == Vector3.ZERO: # just knocked up: it tumbles the way it was hit
		var across := Vector3(velocity.x, 0.0, velocity.z)
		_spin = Vector3.UP.cross(across.normalized()) * maxf(across.length(), 1.0) * SPIN if across.length() > 0.1 else Vector3(0.0, 0.0, 0.01)
	velocity.y -= GRAVITY * delta
	position += velocity * delta
	_tumble = Basis(_spin.normalized(), _spin.length() * delta) * _tumble
	var low := _lowest(_tumble)
	if position.y + low < 0.0: # a corner in the ground: lift it out, bounce, skid
		position.y -= position.y + low
		if velocity.y < 0.0:
			velocity.y = -velocity.y * BOUNCE
		velocity.x *= SKID
		velocity.z *= SKID
		_spin *= SKID
		if velocity.length() < 0.8 and _spin.length() < 1.2:
			thrown = false
			_land()


# How far below its middle its lowest corner is, turned by `turn` (negative).
func _lowest(turn: Basis) -> float:
	var low := INF
	for x in [-1.0, 1.0]:
		for y in [-1.0, 1.0]:
			for z in [-1.0, 1.0]:
				low = minf(low, (turn * (_facing * Vector3(HALF.x * x, stand * y, HALF.y * z))).y)
	return low


# Its middle's height when it rests turned by `turn` (its lowest corner on the ground).
func _lowest_rest(turn: Basis) -> float:
	return -_lowest(turn)


# Came down: tipped over far → it lies there (on its side) a while, then gets up.
func _land() -> void:
	_spin = Vector3.ZERO
	var up := _tumble * Vector3.UP
	if up.y < 0.75:
		var level := Vector3(up.x, 0.0, up.z)
		if level.length() < 1e-3:
			level = Vector3.RIGHT
		_lying = Basis(Quaternion(up, level.normalized())) * _tumble # on its side, falling the way it leaned
		_down = LIE
		_rise = 0.0
	else:
		_tumble = Basis.IDENTITY
		position.y = stand


func _get_up(delta: float) -> void:
	if _down > 0.0:
		_down -= delta
		_tumble = _tumble.slerp(_lying, 1.0 - exp(-14.0 * delta)) # thud onto its side
		return
	if _rise < 1.0 and position.y <= stand + 0.01:
		_rise = minf(_rise + delta / GET_UP, 1.0)
		var u := _rise * _rise * (3.0 - 2.0 * _rise)
		_tumble = _lying.slerp(Basis.IDENTITY, u)


# The robot: facing you, tumbled over its middle, lowered so a body lying down rests on the ground;
# the head crushed by a slam; the body and eyes glow on a hit.
func _draw_robot(_flat: float) -> void:
	var to := (_owner.get("player") as Vector3) - position
	to.y = 0.0
	var standing := _spin == Vector3.ZERO and _down <= 0.0 and _rise >= 1.0
	if standing and to.length() > 0.2: # it turns to face you (not while it tumbles or lies there)
		_facing = Basis.looking_at(to.normalized(), Vector3.UP)
	var turned := _tumble * _facing
	mesh.transform = Transform3D(turned, position) # (its physics keeps its corners out of the ground)
	var crush := 0.0 # 1: head flat
	var buckle := 0.0
	if _crushed >= 0.0: # the hammer demo's crush: driven down, then a slow shudder back up
		crush = _crush * 0.85 * exp(-_crushed * 2.6) * cos(_crushed * 11.0)
		buckle = _crush * 0.3 * exp(-_crushed * 3.5) * cos(_crushed * 9.0)
	var body_height := (height - HEAD) * (1.0 - buckle)
	_body.scale = Vector3(1.0 + buckle * 0.7, 1.0 - buckle, 1.0 + buckle * 0.7)
	_body.position = Vector3.UP * (-stand + body_height * 0.5)
	_head.scale = Vector3(1.0 + crush * 0.8, 1.0 - crush, 1.0 + crush * 0.8)
	_head.position = Vector3.UP * (-stand + body_height + HEAD * (1.0 - crush) * 0.5)
	var off := _hurt >= 0.0 and fmod(_hurt * 23.0, 1.0) < 0.45 and fmod(_hurt * 7.0, 1.0) < 0.7 # an uneven, broken stutter
	for eye in _eyes:
		eye.visible = not off
	_robot_material.emission_enabled = flash > 0.0 or highlight > 0.0
	_robot_material.emission = Color(1, 0.3, 0.2) * flash + Color(1.0, 0.55, 0.15) * highlight * 0.35


func _build_robot() -> void:
	mesh.add_child(_robot)
	_robot_material = StandardMaterial3D.new()
	_robot_material.albedo_color = Color(0.3, 0.31, 0.34)
	_robot_material.metallic = 0.6
	_robot_material.roughness = 0.5
	_body = _box(Vector3(BODY.x, height - HEAD, BODY.y), _robot_material)
	var head_material := _robot_material.duplicate() as StandardMaterial3D
	head_material.albedo_color = Color(0.36, 0.37, 0.4)
	_head = _box(Vector3.ONE * HEAD, head_material)
	var eye_material := StandardMaterial3D.new()
	eye_material.albedo_color = Color(1.0, 0.25, 0.1)
	eye_material.emission_enabled = true
	eye_material.emission = Color(1.0, 0.25, 0.1)
	eye_material.emission_energy_multiplier = 3.0
	for x in [-0.13, 0.13]:
		var eye := MeshInstance3D.new()
		var sphere := SphereMesh.new()
		sphere.radius = 0.065
		sphere.height = 0.13
		eye.mesh = sphere
		eye.material_override = eye_material
		eye.position = Vector3(x, 0.05, -HEAD * 0.5) # its face looks along −Z (toward you)
		_head.add_child(eye)
		_eyes.append(eye)


func _box(
		size: Vector3,
		box_material: StandardMaterial3D
) -> MeshInstance3D:
	var box := BoxMesh.new()
	box.size = size
	var instance := MeshInstance3D.new()
	instance.mesh = box
	instance.material_override = box_material
	_robot.add_child(instance)
	return instance
