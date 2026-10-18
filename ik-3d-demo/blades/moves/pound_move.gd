## Ground pound (Z, hammers): you leap straight up, both heads swing up high over your head with you,
## and as you land they come down together in front of you and slam the ground. A shockwave ring
## tears out from the slam: every enemy it reaches is thrown up and away (the closer, the harder),
## the nearest are flattened.
class_name PoundMove
extends HammerMove

var jump := 9.0 ## m/s straight up (the leap takes 2 × jump / gravity)
var over := 2.6 ## m the heads swing up above your head at the top
var ahead := 1.5 ## m in front of you the heads slam down
var radius := 5.5 ## m the shockwave reaches
var power := 14.0 ## m/s it throws the nearest enemies
var ring_speed := 16.0 ## m/s the ring spreads
var bury := 0.4 ## seconds the heads stay in the ground

var _slammed := false ## this pound has hit the ground (one blast, though two heads land)
var _ring: MeshInstance3D
var _ring_time := -1.0 ## seconds since the slam (−1: no ring)


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "pound"


func tunables() -> Array:
	return [
		["jump", 4.0, 16.0, 0.1], ["over", 0.5, 5.0, 0.05], ["ahead", 0.3, 4.0, 0.05],
		["radius", 1.0, 10.0, 0.1], ["power", 2.0, 30.0, 0.5], ["ring_speed", 4.0, 40.0, 0.5], ["bury", 0.0, 1.0, 0.01],
	]


func reset() -> void:
	_ring_time = -1.0
	if _ring != null:
		_ring.visible = false


func start() -> void:
	if not demo.blades_free() or demo.player.y > 0.05:
		return
	_slammed = false
	demo.player_velocity = Vector3(0.0, jump, 0.0)
	var air := 2.0 * jump / BladesDemo.GRAVITY
	var top_height := jump * jump / (2.0 * BladesDemo.GRAVITY)
	var forward := demo.facing
	for blade in demo.blades:
		begin(blade)
		blade.delay = 0.0 # both heads together
		var side := demo.right() * blade.side * 0.45
		var top := demo.player + Vector3.UP * (top_height + BladesDemo.HAND_HEIGHT + over) - forward * 0.9 + side
		var land := demo.player + forward * ahead + side
		land.y = 0.0
		var from := blade.tip
		var up := PackedVector3Array([from, from + Vector3.UP * 1.5 - forward * 0.6, top - forward * 1.2 - Vector3.UP * 0.8, top])
		up.append_array([top, top + forward * 1.8 + Vector3.UP * 0.6, land + Vector3.UP * 2.5, land])
		blade.start_even(Blade.OUT, up, air, "accelerate") # up with you, then hammered down as you land


func hits_during(_phase: int) -> bool:
	return false # the slam does the damage


func next_phase(blade: Blade) -> void:
	if blade.phase == Blade.OUT:
		if not _slammed:
			_slammed = true
			var at := (demo.blades[0].tip + demo.blades[1].tip) * 0.5
			at.y = 0.0
			blast(at, radius, power, 1.6)
			_show_ring(at)
		rebound(blade, bury + 0.3)
	else:
		super(blade)


## The ring spreads and fades.
func tick(delta: float) -> void:
	if _ring_time < 0.0:
		return
	_ring_time += delta
	var size := maxf(_ring_time * ring_speed, 0.05)
	_ring.scale = Vector3(size, 1.0, size)
	var material: StandardMaterial3D = _ring.material_override
	material.albedo_color.a = clampf(1.0 - size / (radius * 1.2), 0.0, 1.0)
	if size > radius * 1.2:
		_ring_time = -1.0
		_ring.visible = false


func _show_ring(at: Vector3) -> void:
	if _ring == null:
		var mesh := TorusMesh.new()
		mesh.inner_radius = 0.9
		mesh.outer_radius = 1.0
		_ring = MeshInstance3D.new()
		_ring.mesh = mesh
		var material := StandardMaterial3D.new()
		material.albedo_color = Color(0.85, 0.75, 0.6)
		material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		_ring.material_override = material
		demo.add_child(_ring)
	_ring.global_position = at + Vector3.UP * 0.05
	_ring.visible = true
	_ring_time = 0.0
