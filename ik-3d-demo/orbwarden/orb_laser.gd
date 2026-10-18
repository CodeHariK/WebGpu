## The Orb Warden's laser sweep, in three beats:
##   telegraph  a red line flickers on the ground across the target's path (TELEGRAPH s) while the
##              eye slides to the line's start and charges up — time to get out of the way
##   fire       a beam from the lens to the ground sweeps along the line (FIRE s, eased), leaving
##              a scorch trail that glows orange and cools to black
##   cool       the beam shuts off, the eye dims (COOL s), then it's ready again
## The beam is a raycast from the lens toward the sweep point, so it stops on whatever ground it
## meets first. `hit` is emitted every firing frame with the burn point. The telegraph line is a
## ribbon draped over the ground (a raycast every LINE_STEP metres), so it rides over rocks.
## With `sparks` set, the burn point throws sparks and molten drips.
class_name OrbLaser
extends Node3D

signal hit(at: Vector3)

enum Stage { IDLE, TELEGRAPH, FIRE, COOL }

const TELEGRAPH := 1.1
const FIRE := 1.5
const COOL := 0.6
const HALF_LENGTH := 6.5 ## the sweep line runs this far either side of the target
const BEAM_RADIUS := 0.07
const GLOW_RADIUS := 0.24
const SCORCH_GAP := 0.28 ## metres between scorch marks
const SCORCH_LIFE := 9.0 ## seconds a mark stays (glows for the first SCORCH_HOT)
const SCORCH_HOT := 1.6
const RED := Color(1.0, 0.1, 0.04)
const HOT := Color(1.0, 0.45, 0.1)
const LINE_STEP := 0.2 ## metres between the telegraph ribbon's ground samples
const LINE_WIDTH := 0.12
const LINE_LIFT := 0.04 ## ribbon height above the ground
const SPARKS_PER_FRAME := 3
const EMBER_GAP := 0.07 ## seconds between molten drips

var eye: OrbEye
var sparks: OrbSparks
var ground_mask := 1
var stage := Stage.IDLE
var burn_point := Vector3.ZERO ## where the beam meets the ground (while firing)

var _time := 0.0
var _from := Vector3.ZERO
var _to := Vector3.ZERO
var _line: MeshInstance3D
var _line_material: StandardMaterial3D
var _core: MeshInstance3D
var _glow: MeshInstance3D
var _spot: OmniLight3D
var _scorches: Array = [] ## [MeshInstance3D, StandardMaterial3D, age]
var _last_scorch := Vector3.INF
var _ember_wait := 0.0


func _ready() -> void:
	top_level = true
	_build()


## Plan a sweep across `target`, at right angles to the line from the warden (`origin`) to it,
## starting on a random side.
func start(origin: Vector3, target: Vector3) -> void:
	if stage != Stage.IDLE:
		return
	var toward := (target - origin) * Vector3(1, 0, 1)
	if toward.length() < 0.1:
		toward = Vector3.FORWARD
	var across := toward.normalized().cross(Vector3.UP) * (1.0 if randf() < 0.5 else -1.0)
	_from = _ground(target + across * HALF_LENGTH + toward.normalized() * 1.5)
	_to = _ground(target - across * HALF_LENGTH - toward.normalized() * 1.5)
	stage = Stage.TELEGRAPH
	_time = 0.0
	_last_scorch = Vector3.INF
	_place_line()


func busy() -> bool:
	return stage != Stage.IDLE


func status() -> String:
	return ["ready", "telegraph", "FIRING", "cooling"][stage]


func _process(delta: float) -> void:
	_time += delta
	_age_scorches(delta)
	match stage:
		Stage.TELEGRAPH:
			var t := _time / TELEGRAPH
			eye.look_target = _from
			eye.charge = t
			_line.visible = true
			var flicker := 0.5 + 0.5 * sin(_time * lerpf(10.0, 40.0, t))
			_line_material.albedo_color = Color(RED, lerpf(0.15, 0.85, t) * lerpf(0.5, 1.0, flicker))
			if _time >= TELEGRAPH:
				stage = Stage.FIRE
				_time = 0.0
		Stage.FIRE:
			var t := smoothstep(0.0, 1.0, _time / FIRE)
			var aim := _from.lerp(_to, t)
			eye.look_target = aim
			eye.charge = 1.0
			_line_material.albedo_color = Color(RED, 0.35 * (1.0 - t))
			_fire_at(aim)
			if _time >= FIRE:
				stage = Stage.COOL
				_time = 0.0
				_beam_visible(false)
				_line.visible = false
		Stage.COOL:
			eye.charge = 1.0 - _time / COOL
			if _time >= COOL:
				eye.charge = 0.0
				stage = Stage.IDLE


# The beam: a ray from the lens toward the sweep point; it stops at the first ground it meets.
func _fire_at(aim: Vector3) -> void:
	var from := eye.lens_position()
	var direction := (aim - from).normalized()
	var query := PhysicsRayQueryParameters3D.create(from, from + direction * 60.0, ground_mask)
	var result := get_world_3d().direct_space_state.intersect_ray(query)
	burn_point = result.position if not result.is_empty() else aim
	var length := from.distance_to(burn_point)
	var basis := _along(direction)
	var middle := from.lerp(burn_point, 0.5)
	var flicker := 1.0 + 0.25 * sin(_time * 70.0)
	_core.global_transform = Transform3D(basis * Basis.from_scale(Vector3(1, length, 1)), middle)
	_glow.global_transform = Transform3D(basis * Basis.from_scale(Vector3(flicker, length, flicker)), middle)
	_beam_visible(true)
	_spot.global_position = burn_point + Vector3.UP * 0.4
	_spot.light_energy = 6.0 * flicker
	if burn_point.distance_to(_last_scorch) >= SCORCH_GAP:
		_add_scorch(burn_point)
		_last_scorch = burn_point
	if sparks != null:
		var splash := (Vector3.UP * 1.5 - direction * Vector3(1, 0, 1)).normalized() # back up the beam
		sparks.burst(burn_point, splash, SPARKS_PER_FRAME, 7.0, 0.8, 0.45, 0.03, burn_point.y)
		_ember_wait -= get_process_delta_time()
		if _ember_wait <= 0.0:
			_ember_wait = EMBER_GAP
			sparks.embers(burn_point + Vector3.UP * 0.05, 1, 2.2, burn_point.y)
	hit.emit(burn_point)


func _add_scorch(at: Vector3) -> void:
	var disc := CylinderMesh.new()
	var size := randf_range(0.3, 0.42)
	disc.top_radius = size
	disc.bottom_radius = size
	disc.height = 0.02
	disc.radial_segments = 12
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.05, 0.04, 0.035)
	material.roughness = 1.0
	material.emission_enabled = true
	material.emission = HOT
	material.emission_energy_multiplier = 4.0
	var mark := MeshInstance3D.new()
	mark.mesh = disc
	mark.material_override = material
	mark.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mark)
	mark.global_position = at + Vector3.UP * (0.012 + randf() * 0.004)
	mark.rotation.y = randf() * TAU
	_scorches.append([mark, material, 0.0])


func _age_scorches(delta: float) -> void:
	for i in range(_scorches.size() - 1, -1, -1):
		var scorch: Array = _scorches[i]
		scorch[2] += delta
		var material: StandardMaterial3D = scorch[1]
		var heat := clampf(1.0 - scorch[2] / SCORCH_HOT, 0.0, 1.0)
		material.emission = HOT.lerp(RED, 1.0 - heat)
		material.emission_energy_multiplier = 4.0 * heat * heat
		if scorch[2] > SCORCH_LIFE: # fade out by shrinking into the ground
			(scorch[0] as Node3D).queue_free()
			_scorches.remove_at(i)
		elif scorch[2] > SCORCH_LIFE - 1.0:
			(scorch[0] as Node3D).scale = Vector3.ONE * (SCORCH_LIFE - scorch[2])


## Cut the beam / telegraph now (scorch marks stay).
func stop() -> void:
	stage = Stage.IDLE
	_beam_visible(false)
	_line.visible = false
	if eye != null:
		eye.charge = 0.0


func clear() -> void:
	for scorch: Array in _scorches:
		(scorch[0] as Node3D).queue_free()
	_scorches.clear()
	stage = Stage.IDLE
	_beam_visible(false)
	_line.visible = false


# The telegraph ribbon: a triangle strip from _from to _to, each sample dropped onto the ground.
func _place_line() -> void:
	var ribbon := _line.mesh as ImmediateMesh
	ribbon.clear_surfaces()
	var along := (_to - _from) * Vector3(1, 0, 1)
	var samples := maxi(ceili(along.length() / LINE_STEP), 2)
	var side := along.normalized().cross(Vector3.UP) * LINE_WIDTH * 0.5
	ribbon.surface_begin(Mesh.PRIMITIVE_TRIANGLE_STRIP)
	for i in samples + 1:
		var point := _ground(_from.lerp(_to, float(i) / samples)) + Vector3.UP * LINE_LIFT
		ribbon.surface_add_vertex(point + side)
		ribbon.surface_add_vertex(point - side)
	ribbon.surface_end()
	_line.global_transform = Transform3D.IDENTITY


func _ground(point: Vector3) -> Vector3:
	var query := PhysicsRayQueryParameters3D.create(point + Vector3.UP * 20.0, point + Vector3.DOWN * 20.0, ground_mask)
	var result := get_world_3d().direct_space_state.intersect_ray(query)
	return result.position if not result.is_empty() else Vector3(point.x, 0.0, point.z)


func _beam_visible(on: bool) -> void:
	_core.visible = on
	_glow.visible = on
	_spot.visible = on


# A basis whose +Y runs along `direction` (cylinders are built along Y).
static func _along(direction: Vector3) -> Basis:
	var y := direction.normalized()
	var x := y.cross(Vector3.UP if absf(y.y) < 0.99 else Vector3.RIGHT).normalized()
	return Basis(x, y, x.cross(y))


func _build() -> void:
	_line_material = _unshaded(Color(RED, 0.0))
	_line_material.cull_mode = BaseMaterial3D.CULL_DISABLED # the ribbon's winding doesn't matter
	_line = MeshInstance3D.new()
	_line.mesh = ImmediateMesh.new()
	_line.material_override = _line_material
	_line.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_line.visible = false
	add_child(_line)
	_core = _cylinder(BEAM_RADIUS, _unshaded(Color(1.0, 0.92, 0.85)))
	var glow := _unshaded(Color(RED, 0.45))
	glow.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	_glow = _cylinder(GLOW_RADIUS, glow)
	_spot = OmniLight3D.new()
	_spot.light_color = HOT
	_spot.omni_range = 4.0
	add_child(_spot)
	_beam_visible(false)


func _cylinder(radius: float, material: Material) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = 1.0
	mesh.radial_segments = 12
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(instance)
	return instance


static func _unshaded(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = color
	return material
