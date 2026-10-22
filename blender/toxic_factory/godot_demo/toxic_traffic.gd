## Trucks: route A trucks come along the road, turn in at the in gate, pause beside the dock to
## load, leave by the out gate and drive off; route B trucks just pass along the far lane. Each
## truck is a PathFollow3D on a Path3D; when one runs off the end of its route it pops back to
## the start (both ends are far off, in the haze).
## Trucks slow down near the gates, keep their distance from the truck ahead, and the gate arms
## lift while a truck is near.
extends Node3D

const Layout := preload("res://toxic_layout.gd")

const SPEED := 8.0  # m/s on the road
const GATE_SPEED := 3.0
const GAP := 13.0  # metres kept behind the truck ahead
const DWELL := 3.5  # seconds at the dock
const ARM_OPEN := deg_to_rad(80.0)

var _routes: Array[Dictionary] = []  # {"path": Path3D, "length": float, "trucks": Array[Dictionary], "dock": float}
var _gates: Array[Dictionary] = []


func build(kit: RefCounted, gates: Array[Dictionary]) -> void:
	_gates = gates
	_add_route(Layout.route_a(), ["Truck_tanker", "Truck_box", "Truck_tanker"], kit, true)
	_add_route(Layout.route_b(), ["Truck_box", "Truck_tanker"], kit, false)


func _add_route(points: PackedVector3Array, pieces: Array, kit: RefCounted, stops: bool) -> void:
	var curve := Curve3D.new()
	curve.bake_interval = 0.5
	for i in points.size():
		var tangent := (points[mini(i + 1, points.size() - 1)] - points[maxi(i - 1, 0)]) * 0.2
		curve.add_point(points[i], -tangent, tangent)
	var path := Path3D.new()
	path.curve = curve
	add_child(path)
	var length := curve.get_baked_length()
	var route := {"path": path, "length": length, "trucks": [], "dock": -1.0, "gated": stops}
	if stops:
		route.dock = curve.get_closest_offset(Vector3(Layout.DOCK_STOP_X, 0, Layout.YARD_LANE_Z))
	for i in pieces.size():
		var follow := PathFollow3D.new()
		follow.rotation_mode = PathFollow3D.ROTATION_Y
		follow.use_model_front = true  # the trucks face +Z
		follow.loop = true
		follow.progress = length * (1.0 - float(i) / pieces.size()) * 0.9
		var body := MeshInstance3D.new()
		body.mesh = kit.mesh(pieces[i])
		body.position = Vector3(0, 0.08, 0)
		follow.add_child(body)
		path.add_child(follow)
		route.trucks.append({"follow": follow, "speed": SPEED, "wait": 0.0, "loaded": false})
	_routes.append(route)


func _process(delta: float) -> void:
	for route in _routes:
		for truck in route.trucks:
			_drive(route, truck, delta)
	_lift_arms(delta)


func _drive(route: Dictionary, truck: Dictionary, delta: float) -> void:
	var follow: PathFollow3D = truck.follow
	var length: float = route.length
	if truck.wait > 0.0:
		truck.wait -= delta
		return
	var target := SPEED
	if route.gated:
		for gate in _gates:
			if follow.global_position.distance_to(gate.centre) < 12.0:
				target = GATE_SPEED
	if route.dock >= 0.0:
		var to_dock := fposmod(route.dock - follow.progress, length)
		if to_dock < 10.0 and not truck.loaded:
			target = minf(target, maxf(1.0, to_dock * 0.6))
			if to_dock < 0.3:
				truck.wait = DWELL
				truck.loaded = true
				truck.speed = 0.0
				return
		elif to_dock > length * 0.5:
			truck.loaded = false  # past the dock: next lap it stops again
	for other in route.trucks:  # don't drive into the truck ahead
		if other == truck:
			continue
		var ahead := fposmod(other.follow.progress - follow.progress, length)
		if ahead < GAP:
			target = 0.0
	truck.speed = move_toward(truck.speed, target, 6.0 * delta)
	follow.progress += truck.speed * delta


func _lift_arms(delta: float) -> void:
	for gate in _gates:
		var near := false
		for route in _routes:
			if not route.gated:
				continue
			for truck in route.trucks:
				if truck.follow.global_position.distance_to(gate.centre) < 10.0:
					near = true
		var arm: Node3D = gate.arm
		arm.rotation.z = move_toward(arm.rotation.z, ARM_OPEN if near else 0.0, 1.6 * delta)
