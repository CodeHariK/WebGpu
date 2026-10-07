## A cheap bat swarm for crowds: no skeletons, no IK, one MultiMesh each for bodies, left wings,
## right wings and glowing eyes, so any number of bats is 4 draw calls. Each bat is a boid
## (separation, alignment, cohesion) that also rides a wobbling ring round the target. Wings are
## flat scalloped plates that rotate at the shoulder: flapping fast when level or climbing, held
## up in a glide when diving. Each bat banks into its turns.
##   circle   a churning cloud round the target
##   attack   every `attack_every` seconds one bat peels off, dives through the target, rejoins
##   scatter  everyone flees the target, then drifts back and regroups (scatter())
## Positions are in this node's space, so leave it unrotated (anywhere is fine).
class_name BatSwarm
extends Node3D

const SHOULDER := Vector3(0.045, 0.012, -0.015) ## right shoulder on the swarm bat's body
const SCATTER_TIME := 2.5

@export var target: Node3D
@export_range(1, 400) var count := 40:
	set(value):
		count = value
		if is_inside_tree():
			_spawn()
@export var radius := 3.2 ## of the ring round the target
@export var height := 1.4 ## ring height above the target
@export var speed := Vector2(1.8, 3.8) ## min / max cruise speed, m/s
@export var attack_every := 1.2
@export var bat_scale := 1.0
@export var fur_color := Color(0.3, 0.22, 0.26)
@export var wing_color := Color(0.3, 0.18, 0.34)
@export var eye_color := Color(1.0, 0.8, 0.25)

var mode := "circle" ## circle | attack (scatter is temporary, see scatter())

var _layers: Array[MultiMeshInstance3D] = [] ## bodies, eyes, left wings, right wings
var _pos := PackedVector3Array()
var _vel := PackedVector3Array()
var _accel := PackedVector3Array()
var _flap := PackedFloat32Array() ## per-bat flap clock (cycles)
var _glide := PackedFloat32Array() ## 0 flapping … 1 gliding, eased
var _roll := PackedFloat32Array()
var _lane := PackedFloat32Array() ## ring radius multiplier, so the cloud has depth
var _lift := PackedFloat32Array() ## ring height offset
var _dive := PackedFloat32Array() ## seconds left of a dive at the target (attack)
var _time := 0.0
var _attack_clock := 0.0
var _scatter_left := 0.0


func _ready() -> void:
	_layers = [
		_layer(_body_mesh(), BatMeshes.material(fur_color)),
		_layer(_eyes_mesh(), BatMeshes.material(eye_color, 3.0)),
		_layer(_wing_mesh(-1.0), BatMeshes.material(wing_color, 0.0, true)),
		_layer(_wing_mesh(1.0), BatMeshes.material(wing_color, 0.0, true)),
	]
	_spawn()


## Everyone flees the target for a few seconds, then regroups.
func scatter() -> void:
	_scatter_left = SCATTER_TIME
	_dive.fill(0.0)


func _process(delta: float) -> void:
	if delta <= 0.0:
		return
	_time += delta
	_scatter_left -= delta
	var centre := to_local(target.global_position) if target != null else Vector3.ZERO
	_attack_clock -= delta
	if mode == "attack" and _scatter_left <= 0.0 and _attack_clock <= 0.0:
		_attack_clock = attack_every
		_dive[randi() % count] = 1.6
	for i in count:
		_steer(i, delta, centre)
	for i in count:
		_place(i, delta)


# --- flocking -----------------------------------------------------------------------------------

func _steer(i: int, delta: float, centre: Vector3) -> void:
	var p := _pos[i]
	var v := _vel[i]
	var separation := Vector3.ZERO
	var heading := Vector3.ZERO
	var middle := Vector3.ZERO
	var neighbours := 0
	for j in count: # O(n²): fine for a few hundred; use a grid beyond that
		if j == i:
			continue
		var offset := p - _pos[j]
		var distance_squared := offset.length_squared()
		if distance_squared < 2.25:
			heading += _vel[j]
			middle += _pos[j]
			neighbours += 1
			if distance_squared < 0.16 and distance_squared > 1e-6:
				separation += offset / distance_squared
	var desired := _desired_velocity(i, p, centre)
	var top_speed := speed.y * (1.5 if _dive[i] > 0.0 or _scatter_left > 0.0 else 1.0)
	var force := (desired - v) * 2.0 + separation * 0.5
	if neighbours > 0:
		force += (heading / neighbours - v) * 0.4 + (middle / neighbours - p) * 0.2
	if p.y < centre.y - 0.6: # keep off the ground
		force.y += (centre.y - 0.6 - p.y) * 20.0
	var new_velocity := v + force * delta
	new_velocity = new_velocity.normalized() * clampf(new_velocity.length(), speed.x, top_speed)
	_accel[i] = (new_velocity - v) / delta
	_vel[i] = new_velocity
	_pos[i] = p + new_velocity * delta


func _desired_velocity(i: int, p: Vector3, centre: Vector3) -> Vector3:
	if _scatter_left > 0.0:
		var away := p - centre
		away.y = maxf(away.y, 0.0) + 0.6
		return away.normalized() * speed.y * 1.4
	if _dive[i] > 0.0:
		_dive[i] -= get_process_delta_time()
		var to_target := centre - p
		if to_target.length() < 0.35: # through it: carry on a moment, then rejoin
			_dive[i] = minf(_dive[i], 0.3)
		return to_target.normalized() * speed.y * 1.4
	var out := Vector3(p.x - centre.x, 0.0, p.z - centre.z)
	out = out.normalized() if out.length() > 0.01 else Vector3.RIGHT
	var ring := centre + out * radius * _lane[i] + Vector3.UP * (height + _lift[i] + 0.3 * sin(_time * 0.8 + i))
	return Vector3.UP.cross(out) * speed.x * 1.3 + (ring - p) * 1.2


# --- drawing ------------------------------------------------------------------------------------

func _place(i: int, delta: float) -> void:
	var v := _vel[i]
	var forward := v.normalized()
	if absf(forward.y) > 0.97: # looking_at can't take straight up / down
		forward = (forward + Vector3(0.0, 0.0, 0.3)).normalized()
	var basis := Basis.looking_at(forward)
	var sideways := (basis.inverse() * _accel[i]).x
	_roll[i] = lerpf(_roll[i], clampf(-sideways * 0.12, -1.1, 1.1), 1.0 - exp(-6.0 * delta))
	basis = (basis * Basis(Vector3.BACK, _roll[i])).scaled(Vector3.ONE * bat_scale)
	var gliding := 1.0 if v.y < -0.8 else 0.0
	_glide[i] = lerpf(_glide[i], gliding, 1.0 - exp(-8.0 * delta))
	_flap[i] = fmod(_flap[i] + delta * (9.0 + 4.0 * clampf(v.y, 0.0, 1.0)), 1.0)
	var angle := lerpf(0.25 + 0.75 * sin(TAU * _flap[i]), 0.3, _glide[i])
	var body := Transform3D(basis, _pos[i])
	_layers[0].multimesh.set_instance_transform(i, body)
	_layers[1].multimesh.set_instance_transform(i, body)
	_layers[2].multimesh.set_instance_transform(i, body * Transform3D(Basis(Vector3.BACK, -angle), Vector3(-SHOULDER.x, SHOULDER.y, SHOULDER.z)))
	_layers[3].multimesh.set_instance_transform(i, body * Transform3D(Basis(Vector3.BACK, angle), SHOULDER))


func _spawn() -> void:
	_pos.resize(count) # packed arrays are values: resize each one by name
	_vel.resize(count)
	_accel.resize(count)
	_flap.resize(count)
	_glide.resize(count)
	_roll.resize(count)
	_lane.resize(count)
	_lift.resize(count)
	_dive.resize(count)
	var centre := to_local(target.global_position) if target != null else Vector3.ZERO
	for i in count:
		var angle := randf() * TAU
		_lane[i] = randf_range(0.7, 1.3)
		_lift[i] = randf_range(-0.5, 0.6)
		_pos[i] = centre + Vector3(sin(angle) * radius * _lane[i], height + _lift[i], cos(angle) * radius * _lane[i])
		_vel[i] = Vector3(cos(angle), 0.0, -sin(angle)) * speed.x
		_accel[i] = Vector3.ZERO
		_flap[i] = randf()
		_glide[i] = 0.0
		_roll[i] = 0.0
		_dive[i] = 0.0
	for layer in _layers:
		layer.multimesh.instance_count = count
	for i in count:
		_place(i, 0.016)


func _layer(mesh: Mesh, material: Material) -> MultiMeshInstance3D:
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.mesh = mesh
	var layer := MultiMeshInstance3D.new()
	layer.multimesh = multimesh
	layer.material_override = material
	add_child(layer)
	return layer


# Body, head and ears merged into one mesh.
func _body_mesh() -> Mesh:
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	var ball := BatMeshes.sphere(1.0)
	tool.append_from(ball, 0, Transform3D(Basis.from_scale(Vector3(0.055, 0.05, 0.075)), Vector3.ZERO))
	tool.append_from(ball, 0, Transform3D(Basis.from_scale(Vector3.ONE * 0.045), Vector3(0, 0.02, -0.07)))
	for side: float in [-1.0, 1.0]:
		var ear := Basis(Vector3.BACK, -side * 0.4)
		tool.append_from(BatMeshes.cone(0.022, 0.07), 0, Transform3D(ear, Vector3(side * 0.024, 0.07, -0.065)))
	return tool.commit()


func _eyes_mesh() -> Mesh:
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	for side: float in [-1.0, 1.0]:
		tool.append_from(BatMeshes.sphere(0.012), 0, Transform3D(Basis.IDENTITY, Vector3(side * 0.02, 0.03, -0.108)))
	return tool.commit()


# A flat scalloped wing, pivot at the shoulder, reaching along +X (or −X for side −1).
func _wing_mesh(side: float) -> Mesh:
	var outline := PackedVector3Array()
	for point: Vector2 in [
		Vector2(0.0, -0.02), Vector2(0.09, -0.055), Vector2(0.19, -0.05), Vector2(0.26, -0.01),
		Vector2(0.2, 0.0), Vector2(0.17, 0.055), Vector2(0.12, 0.03), Vector2(0.08, 0.075), Vector2(0.0, 0.045),
	]:
		outline.append(Vector3(point.x * side, 0.0, point.y))
	return BatMeshes.fan(Vector3(0.07 * side, 0.0, 0.0), outline, Vector3.UP)
