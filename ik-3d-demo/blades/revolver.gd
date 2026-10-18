## The revolver (weapon mode "revolver", M): a heavy blackened six-shooter with a bone grip, held
## out in your right hand toward the mouse. Left click shoots, right click fans the hammer — every
## round left in the cylinder in a quick, wild burst. Six rounds; empty, the cylinder swings out,
## spins and swings back in (the reload), and it's full again.
##
## Each shot is instant (a hitscan along the aim): the first robot on the line is struck — knocked
## back, dented, sparks and scrap off it — and a tracer streaks to it. The muzzle flashes (a
## burst of light and a flare), and the recoil is heavy: the gun kicks up and back on a spring,
## the arm with it, your body rocks back, you're shoved back a step, the camera jolts. The
## cylinder turns one chamber per shot and the hammer snaps forward, then cocks again.
##
## Its model is built with the barrel along −Z (it points down its forward); place() puts it in
## the hand and turns it to the aim, adding the kick.
class_name Revolver
extends Node3D

const ROUNDS := 6
const COCK := 0.3 ## seconds between aimed shots (cocking the hammer)
const FAN_GAP := 0.09 ## seconds between shots when fanning …
const FAN_SPREAD := 7.0 ## … and how far (degrees) they stray
const RELOAD := 1.0 ## seconds: swing out, spin, swing in
const RANGE := 40.0
const HIT := 0.55 ## m: a shot this close to a robot's middle (plus some for its size) hits it
const KICK_UP := 38.0 ## degrees the muzzle kicks up at full recoil …
const KICK_BACK := 0.16 ## … and m the gun comes back
const PUSH := 1.6 ## m/s each shot shoves you back
const SIZE := 1.5 ## a big, heavy gun
const STEEL := Color(0.08, 0.08, 0.09)
const BONE := Color(0.82, 0.78, 0.68)

var ammo := ROUNDS
var kick := 0.0 ## the recoil now (0 at rest; a shot adds 1; it springs back)

var _demo: BladesDemo
var _kick_speed := 0.0
var _cooldown := 0.0
var _fanning := 0 ## shots still to come in a fan
var _fan_wait := 0.0
var _reload := -1.0 ## seconds into the reload (−1: not reloading)
var _turn := 0.0 ## the cylinder's turn (radians), easing to _turn_goal
var _turn_goal := 0.0
var _hammer_down := 0.0 ## 1 just after a shot (hammer forward), back to 0 as it cocks
var _shove := Vector3.ZERO ## the push back, dying away
var _flash := 0.0 ## the muzzle flash, dying away
var _tracers: Array = [] ## [from, to, age]
var _cylinder := Node3D.new()
var _swing := Node3D.new() ## the crane the cylinder swings out on
var _hammer := Node3D.new()
var _flare: MeshInstance3D
var _light := OmniLight3D.new()
var _lines := ImmediateMesh.new()


func setup(demo: BladesDemo) -> void:
	_demo = demo
	_build()
	var view := MeshInstance3D.new()
	view.mesh = _lines
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	view.material_override = material
	view.top_level = true
	add_child(view)
	visible = false


## For the HUD: the rounds left (or reloading).
func status() -> String:
	if _reload >= 0.0:
		return "reloading…"
	return "●".repeat(ammo) + "○".repeat(ROUNDS - ammo)


## Where your right hand holds it: out in front of your shoulder toward the aim, kicked up and back
## by the recoil (the arm reaches for this: BladesDemo.rest).
func hold_point() -> Vector3:
	var aim := _aim_direction()
	var shoulder := _demo.player + Vector3.UP * 1.45 + _demo.right() * 0.22
	return shoulder + aim * 0.62 + Vector3.UP * kick * 0.22 - aim * kick * KICK_BACK


## Shoot (left click): one aimed round.
func fire() -> void:
	if _cooldown > 0.0 or _reload >= 0.0 or _fanning > 0:
		return
	if ammo <= 0:
		_start_reload()
		return
	_shoot(0.0)
	_cooldown = COCK


## Fan the hammer (right click): every round left, fast and wild.
func fan() -> void:
	if _reload >= 0.0 or _fanning > 0 or ammo <= 0:
		return
	_fanning = ammo
	_fan_wait = 0.0


## A game step: the recoil springs back, the cylinder turns, a fan goes on, the reload plays, the
## shove pushes you back.
func update(delta: float) -> void:
	if delta <= 0.0:
		return
	_cooldown = maxf(_cooldown - delta, 0.0)
	_kick_speed += (-kick * 260.0 - _kick_speed * 22.0) * delta # a stiff, well-damped spring
	kick = maxf(kick + _kick_speed * delta, -0.15)
	_hammer_down = maxf(_hammer_down - delta / COCK, 0.0)
	_turn = lerpf(_turn, _turn_goal, 1.0 - exp(-30.0 * delta))
	_flash = maxf(_flash - delta / 0.06, 0.0)
	if _shove.length() > 0.01:
		_demo.player += _shove * delta
		_shove *= exp(-9.0 * delta)
	if _fanning > 0:
		_fan_wait -= delta
		if _fan_wait <= 0.0:
			_shoot(FAN_SPREAD)
			_fanning -= 1
			_fan_wait = FAN_GAP
	if _reload >= 0.0:
		_reload += delta
		if _reload >= RELOAD:
			_reload = -1.0
			ammo = ROUNDS
	elif ammo == 0 and _cooldown <= 0.0 and _fanning == 0:
		_start_reload()
	for tracer: Array in _tracers:
		tracer[2] += delta
	_tracers = _tracers.filter(func(t: Array) -> bool: return t[2] < 0.08)


## The frame: in your hand, pointing at the aim, kicked up by the recoil; the cylinder, hammer,
## crane, flash and tracers.
func place() -> void:
	var aim := _aim_direction()
	var right := aim.cross(Vector3.UP).normalized()
	var up := right.cross(aim)
	var basis := Basis(right, up, -aim).rotated(right, deg_to_rad(KICK_UP) * kick) # the muzzle kicks up
	global_transform = Transform3D(basis.scaled(Vector3.ONE * SIZE), _demo.grip(1.0))
	_cylinder.rotation = Vector3(0.0, 0.0, _turn + _reload_spin())
	_swing.rotation = Vector3(0.0, 0.0, -1.4 * _swung_out())
	_hammer.rotation = Vector3(-0.7 * (1.0 - _hammer_down), 0.0, 0.0) # cocked back, snapping forward on a shot
	_flare.visible = _flash > 0.0
	_flare.scale = Vector3.ONE * (0.6 + _flash)
	_light.light_energy = 8.0 * _flash
	_lines.clear_surfaces()
	if _tracers.is_empty():
		return
	_lines.surface_begin(Mesh.PRIMITIVE_LINES)
	for tracer: Array in _tracers:
		var fade: float = 1.0 - tracer[2] / 0.08
		_lines.surface_set_color(Color(1.0, 0.85, 0.5, fade))
		_lines.surface_add_vertex(tracer[0])
		_lines.surface_set_color(Color(1.0, 0.5, 0.2, fade * 0.3))
		_lines.surface_add_vertex(tracer[1])
	_lines.surface_end()


func clear() -> void:
	ammo = ROUNDS
	kick = 0.0
	_kick_speed = 0.0
	_fanning = 0
	_reload = -1.0
	_shove = Vector3.ZERO
	_tracers.clear()


# --- the shot -----------------------------------------------------------------------------------

func _shoot(spread: float) -> void:
	ammo -= 1
	var muzzle := _muzzle()
	var aim := _aim_direction()
	if spread > 0.0:
		var stray := deg_to_rad(spread)
		aim = aim.rotated(Vector3.UP, randf_range(-stray, stray)).rotated(aim.cross(Vector3.UP).normalized(), randf_range(-stray, stray) * 0.6).normalized()
	var end := muzzle + aim * RANGE
	var hit := _first_hit(muzzle, end)
	if hit >= 0:
		var enemy := _demo.enemies[hit]
		end = Geometry3D.get_closest_point_to_segment(enemy.position, muzzle, end)
		var push := Vector3(aim.x, 0.0, aim.z).normalized()
		enemy.velocity += push * 9.0 + Vector3.UP * 3.0
		enemy.flash = 1.0
		enemy.squash = maxf(enemy.squash, 0.5)
		_demo.fire.spark(end)
		_demo.impacts.impact(end, 4, 22, 6.0, Color(0.3, 0.32, 0.35), 0.0, false)
		_demo.hitstop = maxf(_demo.hitstop, 0.05)
	_tracers.append([muzzle, end, 0.0])
	# the recoil: gun up and back, body rocked back, shoved a step, camera jolt, the flash
	_kick_speed += 26.0
	_hammer_down = 1.0
	_turn_goal += TAU / ROUNDS
	_flash = 1.0
	var back := -Vector3(aim.x, 0.0, aim.z).normalized()
	_shove += back * PUSH
	_demo.body.pose.kick_back(back * 1.4)
	_demo.shake = maxf(_demo.shake, 0.28)
	_demo.impacts.impact(muzzle, 0, 8, 3.0, Color.BLACK, 0.0, false) # sparks out of the barrel


func _start_reload() -> void:
	_reload = 0.0


# The first robot on the line a → b (nearest to a), −1: none.
func _first_hit(
		a: Vector3,
		b: Vector3
) -> int:
	var best := -1
	var best_distance := INF
	for i in _demo.enemies.size():
		var enemy := _demo.enemies[i]
		var closest := Geometry3D.get_closest_point_to_segment(enemy.position, a, b)
		if closest.distance_to(enemy.position) < HIT + enemy.stand * 0.35:
			var d := a.distance_to(closest)
			if d < best_distance:
				best = i
				best_distance = d
	return best


# The aim: from your shoulder to the robot under the mouse (its middle), else the mouse point.
func _aim_direction() -> Vector3:
	var enemy := _demo.enemy_near_mouse(INF, false)
	var target := _demo.enemies[enemy].position if enemy >= 0 else _demo.mouse_point()
	var from := _demo.player + Vector3.UP * 1.45 + _demo.right() * 0.22
	var to := target - from
	return to.normalized() if to.length() > 0.3 else _demo.facing


func _muzzle() -> Vector3:
	return global_transform * Vector3(0.0, 0.07, -0.42)


# The reload: the crane swings out (0 → 1), stays while the cylinder spins, swings back in.
func _swung_out() -> float:
	if _reload < 0.0:
		return 0.0
	var u := _reload / RELOAD
	return clampf(minf(u / 0.2, (1.0 - u) / 0.2), 0.0, 1.0)


func _reload_spin() -> float:
	if _reload < 0.0:
		return 0.0
	return _reload * 30.0 # a hard flick: it whirls


# --- the model (barrel along −Z, the grip down and back) -----------------------------------------

func _build() -> void:
	_box(Vector3(0.07, 0.11, 0.2), Vector3(0.0, 0.04, 0.0), STEEL) # the frame
	var barrel := _cylinder_mesh(0.022, 0.32, Vector3(0.0, 0.07, -0.25), STEEL)
	barrel.rotation = Vector3(PI * 0.5, 0.0, 0.0)
	_box(Vector3(0.012, 0.025, 0.02), Vector3(0.0, 0.1, -0.39), STEEL) # the front sight
	var grip := _box(Vector3(0.05, 0.16, 0.07), Vector3(0.0, -0.06, 0.1), BONE) # the bone grip, raked back
	grip.rotation = Vector3(-0.35, 0.0, 0.0)
	add_child(_swing)
	_swing.position = Vector3(-0.035, 0.0, -0.02) # the crane's pivot, under the cylinder's left side
	_swing.add_child(_cylinder)
	_cylinder.position = Vector3(0.035, 0.05, 0.0)
	var drum := _cylinder_mesh(0.052, 0.12, Vector3.ZERO, STEEL, _cylinder)
	drum.rotation = Vector3(PI * 0.5, 0.0, 0.0)
	for k in ROUNDS: # the chambers, round the drum's face
		var a := k * TAU / ROUNDS
		var chamber := _cylinder_mesh(0.012, 0.125, Vector3(cos(a), sin(a), 0.0) * 0.032, Color(0.02, 0.02, 0.02), _cylinder)
		chamber.rotation = Vector3(PI * 0.5, 0.0, 0.0)
	add_child(_hammer)
	_hammer.position = Vector3(0.0, 0.09, 0.09)
	var spur := _box(Vector3(0.02, 0.05, 0.02), Vector3(0.0, 0.025, 0.0), STEEL)
	spur.reparent(_hammer, false)
	_flare = MeshInstance3D.new() # the muzzle flash: a hot star at the barrel's end
	var star := SphereMesh.new()
	star.radius = 0.09
	star.height = 0.25
	_flare.mesh = star
	var hot := StandardMaterial3D.new()
	hot.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	hot.albedo_color = Color(1.0, 0.85, 0.45)
	_flare.material_override = hot
	_flare.position = Vector3(0.0, 0.07, -0.5)
	_flare.rotation = Vector3(PI * 0.5, 0.0, 0.0)
	add_child(_flare)
	_light.light_color = Color(1.0, 0.7, 0.35)
	_light.omni_range = 5.0
	_light.position = Vector3(0.0, 0.07, -0.5)
	add_child(_light)


func _box(
		size: Vector3,
		at: Vector3,
		color: Color
) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	return _part(mesh, at, color, self)


func _cylinder_mesh(
		radius: float,
		height: float,
		at: Vector3,
		color: Color,
		parent: Node3D = null
) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	return _part(mesh, at, color, parent if parent != null else self)


func _part(
		mesh: Mesh,
		at: Vector3,
		color: Color,
		parent: Node3D
) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = 0.0 if color == BONE else 0.8
	material.roughness = 0.6 if color == BONE else 0.35
	instance.material_override = material
	instance.position = at
	parent.add_child(instance)
	return instance
