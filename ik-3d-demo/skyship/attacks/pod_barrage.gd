## Pod barrage: launch pods burst up out of the floor at a few random spots round you — each marked
## first by a red ring on the floor where it will come up — then each one throws missiles at you,
## one after another. The missiles snake: a high Bezier arc out of the tube with a sine wave
## across it, so you can't read their line from the missile itself — read the ring that appears
## under you instead (where each will land, filling up as it comes) and keep moving: each pod
## aims at where you are when it fires, so standing still eats the whole salvo.
## The pods sink back into the floor at RECOVER.
##
## params: pods (how many), near / far (m from you they come up), salvo (missiles per pod),
##         gap (s between one pod's missiles), flight (s each missile flies), arc (m high),
##         wave (m side to side), waves (half-swings), + the stage lengths.
class_name PodBarrage
extends ShipAttack

const MAX_PODS := 8
const POD_HEIGHT := 2.4 ## m (scaled)

var _pods: Array[Node3D] = []
var _rings: Array[MeshInstance3D] = []
var _spots: Array[Vector3] = []
var _shots_left: Array[int] = []
var _wait: Array[float] = []
var _ring_material := ShipKit.ghost(Color(ShipKit.RED, 0.0))
var _tip_material := ShipKit.glow(ShipKit.RED, 3.0)


func _init() -> void:
	key = "pods"
	title = "pod barrage"


func _build() -> void:
	var body := ShipKit.metal(Color(0.3, 0.3, 0.33))
	for i in MAX_PODS:
		var pod := Node3D.new()
		add_child(pod)
		pod.scale = Vector3.ONE * 1.5 # big enough to read from the wide camera
		ShipKit.add(pod, ShipKit.cylinder(0.55, 0.7, 0.4, 10), ShipKit.metal(ShipKit.DARK), Vector3.UP * 0.2)
		ShipKit.add(pod, ShipKit.cylinder(0.35, 0.45, POD_HEIGHT / 1.5, 10), body, Vector3.UP * POD_HEIGHT / 1.5 * 0.5)
		ShipKit.add(pod, ShipKit.cylinder(0.3, 0.3, 0.12, 10), _tip_material, Vector3.UP * POD_HEIGHT / 1.5)
		_pods.append(pod)
		var ring := ShipKit.add(self, ShipKit.ring(1.25, 1.5), _ring_material)
		ring.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		_rings.append(ring)
	_on_stop()


func _on_begin(p: Dictionary) -> void:
	var count := mini(int(p.get("pods", 4)), MAX_PODS)
	var near := float(p.get("near", 9.0))
	var far := float(p.get("far", 15.0))
	var center := ShipKit.flat(demo.player)
	_spots.clear()
	_shots_left.clear()
	_wait.clear()
	var start := randf() * TAU
	for i in count: # spread round you, each a little off its even share of the circle
		var angle := start + TAU * i / count + randf_range(-0.4, 0.4)
		var spot := SkyshipArena.inside(center + Vector3(sin(angle), 0, cos(angle)) * randf_range(near, far), 2.0)
		_spots.append(spot)
		_shots_left.append(int(p.get("salvo", 3)))
		_wait.append(i * 0.12) # they don't all fire at once
	for i in MAX_PODS:
		var on := i < count
		_rings[i].visible = on
		_pods[i].visible = false
		if on:
			_rings[i].position = _spots[i] + Vector3.UP * 0.05
			_rings[i].scale = Vector3(1.0, 0.1, 1.0)


func _tick(now: Stage, t: float, delta: float) -> void:
	var rise := 0.0
	match now:
		Stage.TELEGRAPH:
			_ring_material.albedo_color = Color(ShipKit.RED, 0.4 + 0.5 * (0.5 + 0.5 * sin(time * lerpf(8.0, 30.0, t))))
			rise = clampf((t - 0.6) / 0.4, 0.0, 1.0) # they burst up in the last part of the warning
		Stage.FIRE:
			rise = 1.0
			_ring_material.albedo_color = Color(ShipKit.RED, 0.25)
			_fire(delta)
		Stage.RECOVER:
			rise = 1.0 - t
			_ring_material.albedo_color = Color(ShipKit.RED, 0.25 * (1.0 - t))
	for i in _spots.size():
		_pods[i].visible = rise > 0.0
		_pods[i].position = _spots[i] + Vector3.DOWN * POD_HEIGHT * (1.0 - _ease(rise))
		if now == Stage.TELEGRAPH and rise > 0.0 and rise < 0.3 and ship.sparks != null:
			ship.sparks.burst(_spots[i], Vector3.UP, 3, 5.0, 0.8, 0.4, 0.05, 0.0, Color(0.7, 0.6, 0.5)) # floor breaking


func _fire(delta: float) -> void:
	for i in _spots.size():
		_wait[i] -= delta
		if _shots_left[i] <= 0 or _wait[i] > 0.0:
			continue
		_shots_left[i] -= 1
		_wait[i] = float(params.get("gap", 0.3))
		var tube := _spots[i] + Vector3.UP * (POD_HEIGHT + 0.2)
		var target := SkyshipArena.inside(ShipKit.flat(demo.player) + Vector3(randf_range(-1, 1), 0, randf_range(-1, 1)) * 1.2, 1.0)
		ship.missiles.launch(tube, target, 0.0, float(params.get("flight", 0.8)), 0.0)
		ship.missiles.snake(float(params.get("arc", 7.0)), float(params.get("wave", 1.6)) * (1.0 if randf() < 0.5 else -1.0), float(params.get("waves", 3.0)))
		if ship.sparks != null:
			ship.sparks.burst(tube, Vector3.UP, 10, 6.0, 0.4, 0.3, 0.05, 0.0, Color(1.0, 0.8, 0.5))


static func _ease(x: float) -> float:
	return 1.0 - pow(1.0 - x, 3.0) # fast out of the floor, settling


func _on_stop() -> void:
	for pod in _pods:
		pod.visible = false
	for ring in _rings:
		ring.visible = false
