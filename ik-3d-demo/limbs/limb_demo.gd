## Custom limbs (LimbChain + LimbRenderer) vs Skeleton3D + TwoBoneIK3D (SkeletonLegs), same legs,
## same motion, same look. A grid of 8-legged "creatures" (just legs round an invisible body) step
## in place; switch the backend and add creatures to see what each costs.
## In front: the other custom solvers — a FABRIK tentacle and a stretchy AIM leg chasing the orb.
##   Tab  custom ↔ skeleton      [ ]  creatures −/+ 25      V  vsync on/off
## HUD: fps and frame time (everything: skeleton updates, BoneAttachment syncing, rendering), our
## script time per frame split into solve (move targets + solve) and draw (fill the MultiMeshes),
## draw calls and node count. Vsync starts off so the frame time is honest.
extends Node3D

const LEGS := 8
const LENGTHS := Vector2(0.45, 0.5) ## thigh, shin
const HIP_RADIUS := 0.25
const FOOT_RADIUS := 0.7
const HIP_HEIGHT := 0.6
const SPACING := 2.0
const LEG_RADIUS := 0.03

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var custom := true
var creatures := 25

var _renderer: LimbRenderer
var _leg_chains: Array[LimbChain] = []
var _skeleton_creatures: Array[SkeletonLegs] = []
var _tentacle: LimbChain
var _stretchy: LimbChain
var _orb: Node3D
var _time := 0.0
var _script_ms := 0.0 ## whole script update, smoothed
var _solve_ms := 0.0 ## moving targets + solving (custom) or moving markers (skeleton)
var _draw_ms := 0.0 ## filling the MultiMesh buffers


func _ready() -> void:
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	_add_ground()
	_orb = _add_orb()
	_renderer = LimbRenderer.new()
	_renderer.name = "LimbRenderer"
	add_child(_renderer)
	_rebuild()


func _process(delta: float) -> void:
	_time += delta
	var start := Time.get_ticks_usec()
	_orb.position = Vector3(sin(_time * 0.9) * 1.6, 1.2 + sin(_time * 1.7) * 0.5, 4.0 + cos(_time * 0.6) * 0.6)
	_step_legs()
	var solved := Time.get_ticks_usec()
	_tentacle.target = _orb.position
	_stretchy.target = _orb.position + Vector3(0, -0.4, 0)
	_tentacle.solve()
	_stretchy.solve()
	var drawing := Time.get_ticks_usec()
	_renderer.draw()
	var end := Time.get_ticks_usec()
	_script_ms = lerpf(_script_ms, (end - start) / 1000.0, 0.05)
	_solve_ms = lerpf(_solve_ms, (drawing - start) / 1000.0, 0.05)
	_draw_ms = lerpf(_draw_ms, (end - drawing) / 1000.0, 0.05)
	_update_hud()


# Every creature's legs step in place: feet trace a lifted loop, alternate legs out of phase.
func _step_legs() -> void:
	for c in creatures:
		var origin := _creature_origin(c)
		for leg in LEGS:
			var angle := TAU * leg / LEGS
			var out := Vector3(cos(angle), 0, sin(angle))
			var stride := _time * 4.0 + (PI if leg % 2 == 1 else 0.0) + c * 0.7
			var tangent := Vector3(-out.z, 0, out.x)
			var hip := origin + out * HIP_RADIUS + Vector3(0, HIP_HEIGHT, 0)
			var foot := origin + out * FOOT_RADIUS + tangent * sin(stride) * 0.15 + Vector3(0, maxf(0.0, cos(stride)) * 0.15, 0)
			var pole := hip + out * 0.3 + Vector3(0, 0.6, 0)
			if custom:
				var chain := _leg_chains[c * LEGS + leg]
				chain.root = hip
				chain.target = foot
				chain.pole = pole
				chain.solve()
			else:
				var legs := _skeleton_creatures[c]
				legs.targets[leg].global_position = foot
				legs.poles[leg].global_position = pole


func _rebuild() -> void:
	for legs in _skeleton_creatures:
		legs.queue_free()
	_skeleton_creatures.clear()
	_leg_chains.clear()
	_renderer.clear()
	var upper := Vector2(0.05, LENGTHS.x).length() # SkeletonLegs bends its rest pose slightly
	for c in creatures:
		var color := Color.from_hsv(fmod(c * 0.13, 1.0), 0.5, 0.85)
		if custom:
			for leg in LEGS:
				var chain := LimbChain.new(PackedFloat32Array([upper, LENGTHS.y]))
				chain.radius = LEG_RADIUS
				chain.color = color
				_leg_chains.append(chain)
				_renderer.add(chain)
		else:
			var legs := SkeletonLegs.new()
			legs.lengths = LENGTHS
			legs.radius = LEG_RADIUS
			legs.color = color
			for leg in LEGS:
				var angle := TAU * leg / LEGS
				legs.hips.append(Vector3(cos(angle), 0, sin(angle)) * HIP_RADIUS + Vector3(0, HIP_HEIGHT, 0))
			legs.position = _creature_origin(c)
			add_child(legs)
			_skeleton_creatures.append(legs)
	_add_showcase()


# The two non-two-bone solvers, always custom.
func _add_showcase() -> void:
	var tentacle_lengths := PackedFloat32Array()
	for i in 14:
		tentacle_lengths.append(0.24 - i * 0.008)
	_tentacle = LimbChain.new(tentacle_lengths, Vector3(0.0, 0.05, 3.4), LimbChain.Solver.FABRIK)
	_tentacle.pole = Vector3(0.0, 3.0, 2.0)
	_tentacle.radius = 0.045
	_tentacle.color = Color(0.95, 0.5, 0.7)
	_renderer.add(_tentacle)
	_stretchy = LimbChain.new(PackedFloat32Array([0.9]), Vector3(1.4, 1.8, 3.0), LimbChain.Solver.AIM)
	_stretchy.max_stretch = 1.6
	_stretchy.radius = 0.05
	_stretchy.color = Color(0.5, 0.75, 1.0)
	_renderer.add(_stretchy)


func _creature_origin(c: int) -> Vector3:
	var columns := ceili(sqrt(float(creatures)))
	var row := c / columns
	var column := c % columns
	return Vector3((column - (columns - 1) * 0.5) * SPACING, 0, -row * SPACING)


func _update_hud() -> void:
	hud.text = "%s   creatures %d (%d legs)   %d fps   script %.2f ms (solve %.2f, draw %.2f)   frame %.2f ms   draw calls %d   nodes %d   vsync %s\nTab custom ↔ skeleton    [ ] creatures −/+ 25    V vsync" % [
		"CUSTOM limbs" if custom else "SKELETON3D + TwoBoneIK3D", creatures, creatures * LEGS,
		Engine.get_frames_per_second(), _script_ms, _solve_ms, _draw_ms,
		1000.0 / maxf(Engine.get_frames_per_second(), 1.0),
		Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME),
		Performance.get_monitor(Performance.OBJECT_NODE_COUNT),
		"on" if DisplayServer.window_get_vsync_mode() != DisplayServer.VSYNC_DISABLED else "off",
	]


func _unhandled_key_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	match key.keycode:
		KEY_TAB:
			custom = not custom
			_rebuild()
		KEY_BRACKETLEFT:
			creatures = maxi(creatures - 25, 1)
			_rebuild()
		KEY_BRACKETRIGHT:
			creatures += 25
			_rebuild()
		KEY_V:
			var on := DisplayServer.window_get_vsync_mode() != DisplayServer.VSYNC_DISABLED
			DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED if on else DisplayServer.VSYNC_ENABLED)


func _add_ground() -> void:
	var mesh := PlaneMesh.new()
	mesh.size = Vector2(60, 60)
	var ground := MeshInstance3D.new()
	ground.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.4, 0.45, 0.4)
	ground.material_override = material
	add_child(ground)


func _add_orb() -> Node3D:
	var sphere := SphereMesh.new()
	sphere.radius = 0.12
	sphere.height = 0.24
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.6, 1.0, 0.7)
	material.emission_enabled = true
	material.emission = Color(0.4, 1.0, 0.6)
	material.emission_energy_multiplier = 2.0
	var orb := MeshInstance3D.new()
	orb.mesh = sphere
	orb.material_override = material
	add_child(orb)
	return orb
