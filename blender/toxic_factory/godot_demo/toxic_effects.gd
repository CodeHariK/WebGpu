## Air and water pollution that moves:
##   smoke     GPUParticles3D at every chimney (dark), the cooling tower (pale steam) and the plant's
##             roof vents (small): low-poly puffs that rise, drift with the wind, swell and shrink away
##   smog      a ceiling of big dirty-brown puffs drifting across the sky
##   barrels   rusty barrels floating down the river, bobbing and rolling
extends Node3D

const Layout := preload("res://toxic_layout.gd")
const Palette := preload("res://toxic_palette.gd")

const WIND := Vector3(1.4, 0.0, 0.5)  # m/s the smoke and smog drift
const SMOG_PUFFS := 46
const SMOG_SPAN := 150.0

var _smog: MultiMeshInstance3D
var _smog_puffs: Array[Dictionary] = []  # {"pos": Vector3, "size": float, "bob": float}
var _barrels: Array[Dictionary] = []  # {"node": MeshInstance3D, "x": float, "side": float, "phase": float}
var _time := 0.0


func build(kit: RefCounted, chimneys: Array[Vector3], towers: Array[Vector3], vents: Array[Vector3]) -> void:
	var puff: Mesh = kit.mesh("Puff")
	for top in chimneys:
		_stack(puff, top, 1.9, Color(0.25, 0.23, 0.2), Color(0.62, 0.58, 0.5), 34, 2.2)
	for top in towers:
		_stack(puff, top, 2.6, Color(0.85, 0.85, 0.82), Color(0.95, 0.95, 0.9), 22, 2.0)
	for top in vents:
		_stack(puff, top, 0.9, Color(0.5, 0.55, 0.35), Color(0.75, 0.78, 0.6), 12, 1.2)
	_build_smog(puff)
	_build_barrels(kit)


## One smoking stack: puffs leave the top, rise, drift downwind, swell, then shrink away.
func _stack(puff: Mesh, at: Vector3, size: float, dark: Color, light: Color, amount: int, rise: float) -> void:
	var p := GPUParticles3D.new()
	p.amount = amount
	p.lifetime = 7.0
	p.preprocess = 7.0
	p.position = at
	p.visibility_aabb = AABB(Vector3(-30, -5, -30), Vector3(60, 60, 60))
	var m := ParticleProcessMaterial.new()
	m.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_SPHERE
	m.emission_sphere_radius = 0.5 * size
	m.direction = Vector3.UP
	m.spread = 6.0
	m.initial_velocity_min = rise * 0.8
	m.initial_velocity_max = rise * 1.2
	m.gravity = WIND * 0.35 + Vector3(0, 0.1, 0)  # the wind pushes, warm smoke keeps rising a little
	m.damping_min = 0.25
	m.damping_max = 0.4
	m.scale_min = size * 0.8
	m.scale_max = size * 1.2
	var grow := Curve.new()
	grow.add_point(Vector2(0.0, 0.35))
	grow.add_point(Vector2(0.3, 1.0))
	grow.add_point(Vector2(0.8, 1.5))
	grow.add_point(Vector2(1.0, 0.0))
	var grow_tex := CurveTexture.new()
	grow_tex.curve = grow
	m.scale_curve = grow_tex
	var shade := Gradient.new()
	shade.set_color(0, dark)
	shade.set_color(1, light)
	var shade_tex := GradientTexture1D.new()
	shade_tex.gradient = shade
	m.color_ramp = shade_tex
	p.process_material = m
	p.draw_pass_1 = puff
	p.material_override = Palette.material("puff")
	add_child(p)


func _build_smog(puff: Mesh) -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 7
	for i in SMOG_PUFFS:
		_smog_puffs.append({
			"pos": Vector3(rng.randf_range(-SMOG_SPAN / 2, SMOG_SPAN / 2), rng.randf_range(36.0, 46.0), rng.randf_range(-70.0, 15.0)),
			"size": rng.randf_range(4.0, 9.0), "bob": rng.randf_range(0, TAU),
		})
	var multi := MultiMesh.new()
	multi.transform_format = MultiMesh.TRANSFORM_3D
	multi.mesh = puff
	multi.instance_count = SMOG_PUFFS
	_smog = MultiMeshInstance3D.new()
	_smog.multimesh = multi
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.56, 0.5, 0.4)
	mat.roughness = 1.0
	_smog.material_override = mat
	_smog.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(_smog)


func _build_barrels(kit: RefCounted) -> void:
	for i in 6:
		var node := MeshInstance3D.new()
		node.mesh = kit.mesh("Barrel_rust" if i % 2 else "Barrel_hazard")
		add_child(node)
		_barrels.append({"node": node, "x": -Layout.MAP_HALF + i * 32.0, "side": (i % 3 - 1) * 2.5, "phase": i * 1.7})


func _process(delta: float) -> void:
	_time += delta
	for i in _smog_puffs.size():
		var p: Dictionary = _smog_puffs[i]
		var pos: Vector3 = p.pos
		pos.x += WIND.x * 0.6 * delta
		if pos.x > SMOG_SPAN / 2:
			pos.x -= SMOG_SPAN
		p.pos = pos
		var at := pos + Vector3(0, sin(_time * 0.3 + p.bob) * 0.6, 0)
		_smog.multimesh.set_instance_transform(i, Transform3D(Basis.from_scale(Vector3(1.6, 0.7, 1.2) * p.size), at))
	for b in _barrels:  # floating barrels: downstream, bobbing, rolling
		b.x += 1.0 * delta
		if b.x > Layout.MAP_HALF:
			b.x = -Layout.MAP_HALF
		var z: float = Layout.river_z(b.x) + b.side
		var bob := sin(_time * 1.7 + b.phase)
		var node: MeshInstance3D = b.node
		node.transform = Transform3D(
			Basis(Vector3.UP, b.phase) * Basis(Vector3.RIGHT, PI / 2 + bob * 0.15) * Basis(Vector3.UP, _time * 0.5 + b.phase),
			Vector3(b.x, Layout.WATER_Y + 0.15 + bob * 0.06, z))
