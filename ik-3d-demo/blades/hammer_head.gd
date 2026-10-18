## The hammer head that replaces a blade on the end of its chain (weapon mode "hammers", M): a
## burning iron ball. The ball glows with heat (dull red at rest, the emission flaring orange),
## two dark iron bands and a studded belt run round it, cooler than the ball between them; it
## burns — flames lick up off it and stream behind it as it flies (they're left in the world, not
## carried with it), and it throws a flickering orange light round it. Hung on the chain by an
## iron ring.
##
## Built facing its own frame: the chain comes in along +Y (the ball hangs below the chain's end);
## BladesDemo._aim_head turns it each frame.
class_name HammerHead
extends RefCounted

const RADIUS := 0.4 ## the ball's radius
const LENGTH := RADIUS * 2.0 ## how wide it is across (its smear is this wide)
const IRON := Color(0.12, 0.11, 0.11) ## the bands, studs and ring: cooler, darker iron
const HOT := Color(0.9, 0.16, 0.02) ## the glow of the ball itself: red-hot iron
const FLAMES := 48 ## flame puffs alive at once


## A new head under `parent` (hidden until the hammers are out).
static func build(parent: Node3D) -> Node3D:
	var head := Node3D.new()
	parent.add_child(head)
	var ball := Node3D.new()
	ball.position = Vector3(0, -RADIUS, 0) # its top touches the chain's end
	head.add_child(ball)
	var sphere := SphereMesh.new()
	sphere.radius = RADIUS
	sphere.height = RADIUS * 2.0
	var core := _part(ball, sphere, Color(0.16, 0.12, 0.11), Vector3.ZERO, true)
	var hot: StandardMaterial3D = core.material_override
	hot.emission_enabled = true
	hot.emission = HOT
	hot.emission_energy_multiplier = 0.55
	for y in [-0.45, 0.45]: # two iron bands round it
		var band := TorusMesh.new()
		band.inner_radius = RADIUS * sqrt(1.0 - y * y) * 0.97
		band.outer_radius = RADIUS * sqrt(1.0 - y * y) * 1.05
		_part(ball, band, IRON, Vector3(0, y * RADIUS, 0), true)
	var belt := TorusMesh.new() # an iron belt round the middle …
	belt.inner_radius = RADIUS * 0.97
	belt.outer_radius = RADIUS * 1.07
	_part(ball, belt, IRON, Vector3.ZERO, true)
	for k in 10: # … studded all round
		var a := k * TAU / 10.0
		var stud := _part(ball, SphereMesh.new(), IRON, Vector3(sin(a), 0.0, cos(a)) * RADIUS * 1.06, true)
		stud.scale = Vector3.ONE * RADIUS * 0.22
	for k in 5: # and a few on the bottom (what hits first when it falls)
		var a := k * TAU / 5.0
		var stud := _part(ball, SphereMesh.new(), IRON, (Vector3(sin(a) * 0.5, -0.85, cos(a) * 0.5)).normalized() * RADIUS, true)
		stud.scale = Vector3.ONE * RADIUS * 0.2
	_part(ball, _cylinder(RADIUS * 0.3, 0.08), IRON, Vector3(0, RADIUS * 0.97, 0), true) # where the ring is fixed
	var ring := TorusMesh.new() # the iron ring the chain hangs from
	ring.inner_radius = 0.045
	ring.outer_radius = 0.09
	var hook := _part(head, ring, IRON, Vector3(0, -0.02, 0), true)
	hook.rotation = Vector3(PI * 0.5, 0, 0)
	ball.add_child(_flames())
	var light := OmniLight3D.new() # it lights up what's round it
	light.light_color = Color(1.0, 0.45, 0.15)
	light.light_energy = 1.6
	light.omni_range = 3.0
	ball.add_child(light)
	head.visible = false
	return head


# Flames: soft glowing puffs off the ball's surface that rise, swell and fade from yellow through
# orange to dark red. In world space, so a flying ball leaves a burning streak behind it.
static func _flames() -> GPUParticles3D:
	var flames := GPUParticles3D.new()
	flames.amount = FLAMES
	flames.lifetime = 0.45
	flames.local_coords = false
	var process := ParticleProcessMaterial.new()
	process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_SPHERE_SURFACE
	process.emission_sphere_radius = RADIUS * 0.95
	process.direction = Vector3.UP
	process.spread = 25.0
	process.initial_velocity_min = 0.6
	process.initial_velocity_max = 1.6
	process.gravity = Vector3(0, 2.5, 0) # heat rises
	process.scale_min = 0.6
	process.scale_max = 1.2
	var grow := Curve.new()
	grow.add_point(Vector2(0.0, 0.5))
	grow.add_point(Vector2(0.4, 1.0))
	grow.add_point(Vector2(1.0, 0.2))
	var grow_texture := CurveTexture.new()
	grow_texture.curve = grow
	process.scale_curve = grow_texture
	var fade := Gradient.new()
	fade.set_color(0, Color(1.0, 0.9, 0.4, 1.0))
	fade.set_color(1, Color(0.5, 0.05, 0.0, 0.0))
	fade.add_point(0.35, Color(1.0, 0.45, 0.08, 0.9))
	var fade_texture := GradientTexture1D.new()
	fade_texture.gradient = fade
	process.color_ramp = fade_texture
	flames.process_material = process
	var puff := QuadMesh.new()
	puff.size = Vector2.ONE * RADIUS * 0.9
	var look := StandardMaterial3D.new()
	look.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	look.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	look.vertex_color_use_as_albedo = true
	look.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	look.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	var soft := GradientTexture2D.new() # a round, soft-edged puff
	soft.fill = GradientTexture2D.FILL_RADIAL
	soft.fill_from = Vector2(0.5, 0.5)
	soft.fill_to = Vector2(1.0, 0.5)
	var edge := Gradient.new()
	edge.set_color(0, Color(1, 1, 1, 1))
	edge.set_color(1, Color(1, 1, 1, 0))
	soft.gradient = edge
	look.albedo_texture = soft
	puff.material = look
	flames.draw_pass_1 = puff
	return flames


static func _part(
		parent: Node3D,
		mesh: Mesh,
		color: Color,
		at: Vector3,
		metal: bool
) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.45 if metal else 0.9
	material.metallic = 0.85 if metal else 0.0
	instance.material_override = material
	instance.position = at
	parent.add_child(instance)
	return instance


static func _cylinder(
		radius: float,
		height: float
) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	return mesh
