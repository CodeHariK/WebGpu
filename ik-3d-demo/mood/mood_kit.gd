## Prop kits: one per theme, all dressing the same layout (the path, lamp spots, fences, small
## props, trees and landmarks the demo lays out once), so the themes compare on identical ground.
## Each kit answers the same questions differently: what is a tree here, a lamp, a fence, a small
## prop, a landmark, a path stone. Everything is built from primitives with MoodProps' matte
## materials; glowing parts use emission so the bloom picks them up.
##   spooky     pines, dead trees, lanterns, pumpkins, stone towers
##   candy      lollipops, cotton-candy trees, candy-cane lamps, gumdrops, layered cakes
##   desert     cacti, boulders, torches, clay pots, striped sandstone mesas
##   autumn     orange / red / gold round trees, birches, lanterns, hay and pumpkins, a red barn
##   cyberpunk  towers with neon windows, neon sign poles, glowing crates, a billboard skyscraper
##   solarpunk  green round trees, solar lamps, flower pots, white terraced towers with gardens
##   medieval   oaks and pines, torches, barrels and crates, a castle tower with a flag
##   magical    giant glowing mushrooms, crystal clusters, floating crystal lamps, floating islands
##   toybox     a giant kid's room: block stacks and crayons, balls, toy cars, tops, a train-track path
##   skyislands floating rocks with little trees, cloud tufts, a plank bridge, an island with a cottage
##   jungle     tall canopy trees and big leaves, mossy rocks and stone idols, a step-pyramid temple
##   cloud      cloud puffs and marble columns, golden lamps and urns, a rainbow, a domed temple
##   toxic      dead stumps and rusty pipes, hazard barrels, glowing ooze, smoking cooling towers
class_name MoodKit
extends RefCounted

const P := preload("res://mood/mood_props.gd")


## Kits whose trees sway in the wind (MoodShaders); the rest are rigid (towers, block stacks).
const WINDY := ["spooky", "candy", "desert", "autumn", "solarpunk", "medieval", "magical", "skyislands", "jungle", "cloud", "toxic"]


## Build `kit` under `root` on `layout` (MoodDemo._layout), registering lamps with `look`.
## Then the theme shaders go on (MoodShaders): trees sway, candy turns glossy, magical glows
## breathe, cyberpunk neon buzzes. The path keeps MoodLook's plain material (its colour blends live).
static func build(kit: String, root: Node3D, layout: Dictionary, look: MoodLook) -> void:
	var path := _group(root, "Path")
	var trees := _group(root, "Trees")
	var dressing := _group(root, "Dressing") # lamps, fences, props, landmarks
	for tile: Array in layout.tiles:
		_tile(kit, path, tile[0], tile[1], tile[2], look)
	for i in layout.lamps.size():
		var lamp: Array = layout.lamps[i]
		_lamp(kit, dressing, lamp[0], lamp[1], i, look)
	for run: Array in layout.fences:
		_fence(kit, dressing, run[0], run[1])
	for prop: Array in layout.props:
		_prop(kit, dressing, prop[0], prop[1], prop[2])
	for tree: Array in layout.trees:
		_tree(kit, trees, tree[0], tree[1], tree[2])
	for i in layout.landmarks.size():
		var mark: Array = layout.landmarks[i]
		_landmark(kit, dressing, mark[0], mark[1], i, look)
	if kit == "cyberpunk": # rain puddles mirroring the neon, and a probe for them to reflect
		for k in range(3, layout.tiles.size(), 9):
			var tile: Array = layout.tiles[k]
			var across := Basis(Vector3.UP, tile[1]) * Vector3.RIGHT
			MoodShaders.puddle(path, tile[0] + across * randf_range(-0.6, 0.6), randf_range(0.5, 1.0), randf_range(1.2, 2.0))
		var probe := ReflectionProbe.new()
		probe.size = Vector3(60, 24, 90)
		probe.position = Vector3(0, 4, -35)
		probe.update_mode = ReflectionProbe.UPDATE_ONCE
		probe.max_distance = 80.0
		root.add_child(probe)
	if SHAFTS.has(kit): # light shafts beside the landmarks
		for mark: Array in layout.landmarks:
			_shaft(root, mark[0] + Vector3(0, 6, 2.5), SHAFTS[kit])
	var candy := kit == "candy"
	var pulse := 1.0 if kit == "magical" else 0.0
	var flicker := 1.0 if kit == "cyberpunk" else 0.0
	if WINDY.has(kit) or candy or pulse > 0.0 or flicker > 0.0:
		MoodShaders.dress(trees, candy, WINDY.has(kit), pulse, flicker)
	if candy or pulse > 0.0 or flicker > 0.0:
		MoodShaders.dress(dressing, candy, false, pulse, flicker)


static func _group(root: Node3D, group_name: String) -> Node3D:
	var group := Node3D.new()
	group.name = group_name
	root.add_child(group)
	return group


## Kits with light shafts, and their colour.
const SHAFTS := {"spooky": Color(1.0, 0.6, 0.5), "autumn": Color(1.0, 0.85, 0.55), "magical": Color(0.5, 0.8, 1.0), "medieval": Color(1.0, 0.95, 0.8),
	"jungle": Color(0.9, 1.0, 0.6), "cloud": Color(1.0, 0.95, 0.8), "toybox": Color(1.0, 0.95, 0.8)}
const PRIMARY := [Color(0.95, 0.2, 0.2), Color(1.0, 0.8, 0.1), Color(0.2, 0.45, 0.95), Color(0.2, 0.75, 0.3), Color(1.0, 0.5, 0.1)]


# A light shaft: a tall camera-facing plane with a soft gradient across it, added on top.
static func _shaft(root: Node3D, at: Vector3, color: Color) -> void:
	var gradient := Gradient.new()
	gradient.colors = PackedColorArray([Color(color, 0.0), Color(color, 0.25), Color(color, 0.0)])
	gradient.offsets = PackedFloat32Array([0.0, 0.5, 1.0])
	var texture := GradientTexture2D.new()
	texture.gradient = gradient
	texture.fill_to = Vector2(1, 0)
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	material.albedo_texture = texture
	material.billboard_mode = BaseMaterial3D.BILLBOARD_FIXED_Y
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	var quad := QuadMesh.new()
	quad.size = Vector2(2.5, 14.0)
	var shaft := MeshInstance3D.new()
	shaft.mesh = quad
	shaft.material_override = material
	shaft.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	shaft.position = at
	root.add_child(shaft)


# --- path ---------------------------------------------------------------------------------------

static func _tile(kit: String, root: Node3D, at: Vector3, yaw: float, index: int, look: MoodLook) -> void:
	match kit:
		"toybox": # wooden train track: a tie, two steel rails
			_piece(root, Vector3(1.4, 0.06, 0.2), at + Vector3.UP * 0.03, yaw, look.path_material)
			var across := Basis(Vector3.UP, yaw) * Vector3.RIGHT
			for side: float in [-0.5, 0.5]:
				P.box(root, Vector3(0.07, 0.07, 0.56), at + across * side + Vector3.UP * 0.09, Color(0.6, 0.6, 0.65), Vector3(0, yaw, 0))
		"toxic": # cracked concrete slabs, the odd glowing puddle
			_piece(root, Vector3(2.3, 0.08, 0.5), at + Vector3.UP * 0.04, yaw + randf_range(-0.08, 0.08), look.path_material)
			if index % 17 == 3:
				_cylinder(root, 0.45, 0.45, 0.02, at + Vector3(randf_range(-0.5, 0.5), 0.09, 0), Color(0.6, 1.0, 0.2)).material_override = P.material(Color(0.6, 1.0, 0.2), 2.5)
		"spooky", "autumn", "skyislands":
			if index % 13 != 5: # a missing plank now and then
				_piece(root, Vector3(2.2, 0.08, 0.42), at + Vector3.UP * 0.04, yaw, look.path_material)
		"candy": # wafer slabs
			_piece(root, Vector3(2.2, 0.1, 0.48), at + Vector3.UP * 0.05, yaw, look.path_material)
			if index % 2 == 0:
				P.box(root, Vector3(2.22, 0.03, 0.08), at + Vector3.UP * 0.1, Color(1, 1, 1), Vector3(0, yaw, 0))
		"cyberpunk":
			_piece(root, Vector3(2.4, 0.06, 0.52), at + Vector3.UP * 0.03, yaw, look.path_material)
			if index % 6 == 0: # glowing lane line
				var line := P.box(root, Vector3(2.4, 0.07, 0.05), at + Vector3.UP * 0.035, Color(0.2, 0.9, 1.0), Vector3(0, yaw, 0))
				line.material_override = P.material(Color(0.2, 0.9, 1.0), 3.0)
		_: # stones: two or three per row, a little uneven
			var count := 3 if index % 2 == 0 else 2
			var across := Basis(Vector3.UP, yaw) * Vector3.RIGHT
			for k in count:
				var offset := (float(k) - (count - 1) * 0.5) * (2.2 / count)
				var stone := _piece(root, Vector3(2.2 / count - 0.08, 0.1, 0.46), at + across * offset + Vector3.UP * 0.05, yaw + randf_range(-0.05, 0.05), look.path_material)
				if kit == "magical" and randf() < 0.06: # a glowing rune
					P.box(stone, Vector3(0.18, 0.02, 0.18), Vector3.UP * 0.055, Color(0.4, 0.9, 1.0)).material_override = P.material(Color(0.4, 0.9, 1.0), 2.5)


static func _piece(root: Node3D, size: Vector3, at: Vector3, yaw: float, material: Material) -> MeshInstance3D:
	var piece := P.box(root, size, at, Color.WHITE, Vector3(0, yaw, 0))
	piece.material_override = material
	return piece


# --- lamps --------------------------------------------------------------------------------------

static func _lamp(kit: String, root: Node3D, at: Vector3, yaw: float, index: int, look: MoodLook) -> void:
	var base := _root(root, at, yaw)
	var glow := StandardMaterial3D.new() # its own: MoodLook sets its colour and flicker
	glow.emission_enabled = true
	glow.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var tint: Variant = null
	var light_at := Vector3(0.52, 1.5, 0)
	match kit:
		"candy": # a candy cane with a glowing gumdrop hanging off the hook
			for i in 8:
				P.box(base, Vector3(0.14, 0.24, 0.14), Vector3(0, 0.12 + i * 0.24, 0), Color(1, 1, 1) if i % 2 == 0 else Color(0.95, 0.15, 0.25))
			P.box(base, Vector3(0.5, 0.14, 0.14), Vector3(0.22, 1.95, 0), Color(0.95, 0.15, 0.25))
			_glow_sphere(base, 0.13, Vector3(0.45, 1.72, 0), glow)
			light_at = Vector3(0.45, 1.65, 0)
		"toybox": # a toy lamp: a striped post and a glowing ball
			var color: Color = PRIMARY[index % PRIMARY.size()]
			for i in 4:
				P.box(base, Vector3(0.16, 0.45, 0.16), Vector3(0, 0.23 + i * 0.45, 0), color if i % 2 == 0 else Color(1, 1, 1))
			_glow_sphere(base, 0.2, Vector3(0, 2.0, 0), glow)
			light_at = Vector3(0, 2.0, 0)
		"cloud": # a slim golden post with a glowing orb
			_cylinder(base, 0.06, 0.04, 2.0, Vector3(0, 1.0, 0), Color(0.95, 0.78, 0.35)).material_override = P.material(Color(0.95, 0.78, 0.35), 0.0, 0.6)
			_glow_sphere(base, 0.16, Vector3(0, 2.1, 0), glow)
			light_at = Vector3(0, 2.1, 0)
		"desert", "medieval", "jungle": # a torch: post, iron bowl, flame
			P.box(base, Vector3(0.12, 1.6, 0.12), Vector3(0, 0.8, 0), P.POST)
			P.cone(base, 0.18, 0.2, Vector3(0, 1.7, 0), Color(0.15, 0.13, 0.12)).rotation.x = PI
			_glow_sphere(base, 0.12, Vector3(0, 1.85, 0), glow)
			light_at = Vector3(0, 1.95, 0)
		"cyberpunk": # a pole with a neon sign, alternating magenta / cyan / yellow
			P.box(base, Vector3(0.1, 3.0, 0.1), Vector3(0, 1.5, 0), Color(0.1, 0.1, 0.12))
			tint = [Color(1.0, 0.2, 0.8), Color(0.2, 0.9, 1.0), Color(1.0, 0.85, 0.2)][index % 3]
			var sign := P.box(base, Vector3(0.9, 0.35, 0.06), Vector3(0.5, 2.4, 0), Color.WHITE)
			sign.material_override = glow
			light_at = Vector3(0.5, 2.2, 0.3)
		"solarpunk": # white post, tilted solar panel, a bright bulb
			P.box(base, Vector3(0.1, 2.0, 0.1), Vector3(0, 1.0, 0), Color(0.95, 0.95, 0.92))
			P.box(base, Vector3(0.6, 0.04, 0.4), Vector3(0, 2.05, 0), Color(0.1, 0.18, 0.35), Vector3(0.4, 0, 0))
			_glow_sphere(base, 0.1, Vector3(0.0, 1.8, -0.15), glow)
			light_at = Vector3(0, 1.75, -0.2)
		"magical": # a crystal floating over a mossy stone, alternating cyan / pink
			P.box(base, Vector3(0.5, 0.4, 0.5), Vector3(0, 0.2, 0), Color(0.25, 0.28, 0.35), Vector3(0, 0.4, 0))
			tint = [Color(0.3, 0.9, 1.0), Color(1.0, 0.45, 0.9)][index % 2]
			var crystal := P.box(base, Vector3(0.18, 0.5, 0.18), Vector3(0, 1.2, 0), Color.WHITE, Vector3(0.3, 0.6, 0.2))
			crystal.material_override = glow
			light_at = Vector3(0, 1.2, 0)
		_: # spooky / autumn: the classic lantern post
			P.box(base, Vector3(0.12, 1.9, 0.12), Vector3(0, 0.95, 0), P.POST)
			P.box(base, Vector3(0.6, 0.07, 0.07), Vector3(0.28, 1.8, 0), P.POST)
			P.box(base, Vector3(0.22, 0.28, 0.22), Vector3(0.52, 1.56, 0), P.POST)
			_glow_sphere(base, 0.08, Vector3(0.52, 1.55, 0), glow)
	var light := OmniLight3D.new()
	light.position = light_at
	light.shadow_enabled = false # cheap: only the sun / moon casts shadows
	base.add_child(light)
	look.add_lamp(light, glow, tint)


static func _glow_sphere(parent: Node3D, radius: float, at: Vector3, material: Material) -> MeshInstance3D:
	var shape := SphereMesh.new()
	shape.radius = radius
	shape.height = radius * 2.0
	var instance := MeshInstance3D.new()
	instance.mesh = shape
	instance.material_override = material
	instance.position = at
	parent.add_child(instance)
	return instance


# --- fences -------------------------------------------------------------------------------------

static func _fence(kit: String, root: Node3D, a: Vector3, b: Vector3) -> void:
	var post := P.POST
	var rail := P.POST
	match kit:
		"candy": post = Color(1.0, 0.95, 0.97); rail = Color(1.0, 0.55, 0.75)
		"desert": post = Color(0.5, 0.36, 0.25); rail = Color(0.6, 0.48, 0.32)
		"autumn", "medieval": post = Color(0.45, 0.32, 0.2); rail = Color(0.55, 0.4, 0.26)
		"cyberpunk": post = Color(0.25, 0.25, 0.28); rail = Color(0.95, 0.8, 0.1)
		"solarpunk": post = Color(0.95, 0.95, 0.92); rail = Color(0.95, 0.95, 0.92)
		"magical": post = Color(0.3, 0.32, 0.4); rail = Color(0.3, 0.32, 0.4)
		"toybox": post = Color(1.0, 0.8, 0.1); rail = Color(0.95, 0.2, 0.2)
		"skyislands": post = Color(0.5, 0.35, 0.22); rail = Color(0.8, 0.7, 0.5)
		"jungle": post = Color(0.4, 0.42, 0.34); rail = Color(0.3, 0.45, 0.2)
		"cloud": post = Color(0.98, 0.97, 0.95); rail = Color(0.95, 0.8, 0.4)
		"toxic": post = Color(0.45, 0.28, 0.18); rail = Color(0.95, 0.75, 0.1)
	var count := maxi(int(a.distance_to(b) / 1.6), 1)
	for i in count + 1:
		var at := a.lerp(b, float(i) / count)
		var lean := 0.12 if kit in ["spooky", "magical", "toxic"] else 0.03
		P.box(root, Vector3(0.12, 1.1, 0.12), at + Vector3.UP * 0.5, post, Vector3(randf_range(-lean, lean), 0, randf_range(-lean, lean)))
	if kit == "medieval": # a pennant on the first post
		var pole := P.box(root, Vector3(0.05, 1.2, 0.05), a + Vector3.UP * 1.6, P.POST)
		MoodShaders.flag(pole, Vector3(0, 0.4, 0), Vector2(0.7, 0.35), [Color(0.75, 0.15, 0.12), Color(0.15, 0.3, 0.75)][randi() % 2])
	var middle := a.lerp(b, 0.5)
	var yaw := atan2(b.x - a.x, b.z - a.z)
	var heights: Array = [0.85] if kit == "magical" else [0.45, 0.85]
	for h: float in heights:
		var bar := P.box(root, Vector3(0.06, 0.1, a.distance_to(b)), middle + Vector3.UP * h, rail, Vector3(0, yaw, 0))
		if kit == "cyberpunk":
			bar.material_override = P.material(rail, 1.5)


# --- small props -----------------------------------------------------------------------------------

static func _prop(kit: String, root: Node3D, at: Vector3, size: float, variant: int) -> void:
	match kit:
		"spooky":
			P.pumpkin(root, at, size, variant % 3 == 0)
		"candy": # gumdrops and little cupcakes
			var color: Color = [Color(1.0, 0.4, 0.6), Color(0.5, 0.85, 1.0), Color(0.75, 1.0, 0.5), Color(1.0, 0.85, 0.35)][variant % 4]
			if variant % 2 == 0:
				var drop := P.cone(root, size * 1.1, size * 1.6, at + Vector3.UP * size * 0.8, color)
				drop.material_override = P.material(color, 0.0, 0.6)
			else:
				_cylinder(root, size, size * 0.8, size * 0.9, at + Vector3.UP * size * 0.45, Color(0.85, 0.6, 0.4))
				_sphere(root, size * 0.95, at + Vector3.UP * size * 1.1, color)
				_sphere(root, size * 0.2, at + Vector3.UP * size * 1.95, Color(0.9, 0.1, 0.2))
		"desert": # clay pots and small rocks
			if variant % 2 == 0:
				_cylinder(root, size * 0.8, size * 0.55, size * 2.0, at + Vector3.UP * size, Color(0.75, 0.4, 0.25))
			else:
				_sphere(root, size * 1.4, at + Vector3.UP * size * 0.4, Color(0.7, 0.55, 0.42), Vector3(1, 0.6, 1))
		"autumn": # pumpkins, hay bales, leaf piles
			match variant % 3:
				0: P.pumpkin(root, at, size, false)
				1: P.box(root, Vector3(0.9, 0.5, 0.6) * size * 2.0, at + Vector3.UP * size * 0.5, Color(0.85, 0.7, 0.3), Vector3(0, randf() * TAU, 0))
				2: _sphere(root, size * 2.5, at, Color(0.85, 0.35, 0.1), Vector3(1, 0.3, 1))
		"cyberpunk": # crates with a glowing strip, vending machines
			if variant % 3 == 0:
				var machine := P.box(root, Vector3(0.7, 1.6, 0.6), at + Vector3.UP * 0.8, Color(0.12, 0.12, 0.15), Vector3(0, randf() * TAU, 0))
				P.box(machine, Vector3(0.5, 1.0, 0.02), Vector3(0, 0.15, -0.31), Color(0.2, 0.9, 1.0)).material_override = P.material(Color(0.2, 0.9, 1.0), 2.5)
			else:
				var crate := P.box(root, Vector3.ONE * size * 2.5, at + Vector3.UP * size * 1.25, Color(0.2, 0.2, 0.24), Vector3(0, randf() * TAU, 0))
				P.box(crate, Vector3(size * 2.55, 0.04, size * 2.55), Vector3.ZERO, Color(1.0, 0.2, 0.8)).material_override = P.material(Color(1.0, 0.2, 0.8), 2.5)
		"solarpunk": # white flower pots
			_cylinder(root, size * 0.9, size * 0.7, size * 1.2, at + Vector3.UP * size * 0.6, Color(0.95, 0.95, 0.92))
			_sphere(root, size * 1.0, at + Vector3.UP * size * 1.4, Color(0.35, 0.65, 0.3))
			for k in 4:
				var flower: Color = [Color(1, 0.4, 0.5), Color(1, 0.85, 0.3), Color(0.6, 0.5, 1.0)][(variant + k) % 3]
				_sphere(root, size * 0.25, at + Vector3(randf_range(-0.6, 0.6) * size, size * 2.0, randf_range(-0.6, 0.6) * size), flower)
		"medieval": # barrels and crates
			if variant % 2 == 0:
				_cylinder(root, size * 1.1, size * 1.1, size * 2.6, at + Vector3.UP * size * 1.3, Color(0.5, 0.33, 0.2))
				for h: float in [0.5, 2.1]:
					_cylinder(root, size * 1.15, size * 1.15, 0.05, at + Vector3.UP * size * h, Color(0.2, 0.2, 0.22))
			else:
				P.box(root, Vector3.ONE * size * 2.4, at + Vector3.UP * size * 1.2, Color(0.55, 0.4, 0.25), Vector3(0, randf() * TAU, 0))
		"toybox": # rubber balls, toy cars, spinning tops
			var color: Color = PRIMARY[variant % PRIMARY.size()]
			match variant % 3:
				0:
					_sphere(root, size * 1.3, at + Vector3.UP * size * 1.3, color).material_override = P.material(color, 0.0, 0.5)
					_torus(root, size * 1.25, size * 1.35, at + Vector3.UP * size * 1.3, Color(1, 1, 1), Vector3(0.3, 0, 0.2))
				1:
					var car := _root(root, at, randf() * TAU)
					P.box(car, Vector3(0.6, 0.25, 1.0) * size * 2.5, Vector3.UP * size * 0.6, color)
					P.box(car, Vector3(0.5, 0.2, 0.5) * size * 2.5, Vector3(0, size * 1.1, size * 0.1), Color(0.8, 0.9, 1.0))
					for wx: float in [-0.35, 0.35]:
						for wz: float in [-0.35, 0.35]:
							_cylinder(car, size * 0.35, size * 0.35, size * 0.25, Vector3(wx * size * 2.5, size * 0.35, wz * size * 2.5), Color(0.1, 0.1, 0.1)).rotation.z = PI * 0.5
				2:
					P.cone(root, size * 1.2, size * 1.2, at + Vector3.UP * size * 1.4, color)
					P.cone(root, size * 1.2, size * 0.8, at + Vector3.UP * size * 0.45, color).rotation.x = PI
		"skyislands": # little clouds drifting low
			for k in 3:
				_sphere(root, size * randf_range(1.5, 2.5), at + Vector3(randf_range(-0.6, 0.6), 0.6 + randf() * 0.4, randf_range(-0.6, 0.6)), Color(1, 1, 1))
		"jungle": # mossy rocks and stone idols with glowing eyes
			if variant % 3 == 0:
				var idol := P.box(root, Vector3(0.7, 0.9, 0.6) * size * 2.5, at + Vector3.UP * size * 1.1, Color(0.45, 0.47, 0.4), Vector3(0, randf() * TAU, 0))
				for x: float in [-0.12, 0.12]:
					P.box(idol, Vector3(0.1, 0.06, 0.02) * size * 2.5, Vector3(x * size * 2.5, size * 0.3, -size * 0.76), Color(0.4, 1.0, 0.5)).material_override = P.material(Color(0.4, 1.0, 0.5), 3.0)
			else:
				_sphere(root, size * 2.0, at + Vector3.UP * size * 0.5, Color(0.42, 0.45, 0.38), Vector3(1, 0.6, 1))
				_sphere(root, size * 1.6, at + Vector3.UP * size * 1.0, Color(0.3, 0.5, 0.22), Vector3(1, 0.3, 1))
		"cloud": # golden urns and cloud tufts
			if variant % 2 == 0:
				_cylinder(root, size * 0.5, size * 0.9, size * 1.6, at + Vector3.UP * size * 0.8, Color(0.95, 0.78, 0.35)).material_override = P.material(Color(0.95, 0.78, 0.35), 0.0, 0.6)
			else:
				for k in 3:
					_sphere(root, size * randf_range(1.2, 2.0), at + Vector3(randf_range(-0.5, 0.5), size, randf_range(-0.5, 0.5)), Color(1, 1, 1))
		"toxic": # hazard barrels with glowing tops, ooze puddles
			if variant % 2 == 0:
				_cylinder(root, size * 1.0, size * 1.0, size * 2.6, at + Vector3.UP * size * 1.3, Color(0.85, 0.7, 0.1)).rotation.z = randf_range(-0.2, 0.2)
				_cylinder(root, size * 0.9, size * 0.9, 0.03, at + Vector3.UP * size * 2.62, Color(0.6, 1.0, 0.2)).material_override = P.material(Color(0.6, 1.0, 0.2), 3.0)
			else:
				_cylinder(root, size * 3.0, size * 3.0, 0.03, at + Vector3.UP * 0.02, Color(0.6, 1.0, 0.2)).material_override = P.material(Color(0.6, 1.0, 0.2), 2.0)
		"magical": # glowing toadstool clusters
			for k in 3:
				var spot := at + Vector3(randf_range(-0.4, 0.4), 0, randf_range(-0.4, 0.4))
				var tall := size * randf_range(1.0, 2.0)
				_cylinder(root, size * 0.18, size * 0.22, tall, spot + Vector3.UP * tall * 0.5, Color(0.85, 0.85, 0.9))
				var cap := _sphere(root, size * 0.7, spot + Vector3.UP * tall, Color(0.4, 1.0, 0.9), Vector3(1, 0.45, 1))
				cap.material_override = P.material([Color(0.4, 1.0, 0.9), Color(1.0, 0.5, 0.95)][variant % 2], 2.0)


# --- trees ----------------------------------------------------------------------------------------

static func _tree(kit: String, root: Node3D, at: Vector3, height: float, variant: int) -> void:
	match kit:
		"spooky":
			if variant % 10 < 7:
				P.pine(root, at, height)
			else:
				P.dead_tree(root, at, height * 0.7)
		"candy":
			if variant % 2 == 0: # lollipop
				_cylinder(root, 0.08, 0.08, height * 0.6, at + Vector3.UP * height * 0.3, Color(1, 1, 1))
				var color: Color = [Color(1.0, 0.4, 0.65), Color(0.45, 0.8, 1.0), Color(1.0, 0.85, 0.35), Color(0.7, 1.0, 0.5)][variant % 4]
				var candy := _sphere(root, height * 0.22, at + Vector3.UP * height * 0.7, color, Vector3(1, 1, 0.35))
				candy.material_override = P.material(color, 0.0, 0.7)
				_torus(root, height * 0.12, height * 0.15, at + Vector3.UP * height * 0.7, Color(1, 1, 1), Vector3(PI * 0.5, 0, 0))
			else: # cotton-candy tree
				_cylinder(root, 0.12, 0.18, height * 0.5, at + Vector3.UP * height * 0.25, Color(0.95, 0.85, 0.75))
				var fluff: Color = [Color(1.0, 0.75, 0.88), Color(0.75, 0.88, 1.0), Color(0.88, 0.8, 1.0)][variant % 3]
				for k in 3:
					_sphere(root, height * randf_range(0.18, 0.26), at + Vector3(randf_range(-0.6, 0.6), height * randf_range(0.55, 0.8), randf_range(-0.6, 0.6)), fluff)
		"desert":
			if variant % 3 != 2: # saguaro cactus
				var green := Color(0.35, 0.55, 0.3)
				_capsule(root, 0.22, height * 0.6, at + Vector3.UP * height * 0.3, green)
				for side: float in [-1.0, 1.0]:
					if randf() < 0.7:
						_capsule(root, 0.15, height * 0.2, at + Vector3(side * 0.45, height * randf_range(0.3, 0.45), 0), green)
						_capsule(root, 0.14, 0.5, at + Vector3(side * 0.25, height * 0.3, 0), green, Vector3(0, 0, PI * 0.5))
			else: # boulders
				for k in 3:
					_sphere(root, height * randf_range(0.12, 0.2), at + Vector3(randf_range(-1, 1), height * 0.08 * k, randf_range(-1, 1)), Color(0.72, 0.5, 0.36), Vector3(1, 0.7, 1))
		"autumn", "solarpunk", "medieval":
			if kit == "medieval" and variant % 3 == 0:
				var pine := P.pine(root, at, height)
				_recolor(pine, Color(0.18, 0.35, 0.2))
				return
			var birch := kit == "autumn" and variant % 4 == 0
			_cylinder(root, 0.14, 0.22, height * 0.55, at + Vector3.UP * height * 0.27, Color(0.92, 0.9, 0.85) if birch else Color(0.38, 0.27, 0.18))
			var leaves: Array
			match kit:
				"autumn": leaves = [Color(0.95, 0.45, 0.1), Color(0.85, 0.2, 0.1), Color(0.95, 0.72, 0.2)]
				"solarpunk": leaves = [Color(0.4, 0.72, 0.3), Color(0.55, 0.8, 0.35), Color(0.3, 0.6, 0.35)]
				_: leaves = [Color(0.3, 0.55, 0.25), Color(0.38, 0.6, 0.28), Color(0.25, 0.48, 0.22)]
			var color: Color = leaves[variant % leaves.size()]
			for k in 4:
				_sphere(root, height * randf_range(0.16, 0.24), at + Vector3(randf_range(-0.7, 0.7), height * randf_range(0.55, 0.85), randf_range(-0.7, 0.7)), color.lightened(randf_range(-0.1, 0.1)))
		"cyberpunk": # towers with neon window strips
			var tall := height * 2.5
			var width := randf_range(2.0, 3.5)
			var tower := P.box(root, Vector3(width, tall, width), at + Vector3.UP * tall * 0.5, Color(0.1, 0.1, 0.13))
			var neon: Color = [Color(1.0, 0.2, 0.8), Color(0.2, 0.9, 1.0), Color(1.0, 0.85, 0.2)][variant % 3]
			for k in int(tall / 2.5):
				if randf() < 0.6:
					var strip := P.box(tower, Vector3(width * 1.01, 0.12, width * 1.01), Vector3(0, -tall * 0.5 + 1.5 + k * 2.5, 0), neon)
					strip.material_override = P.material(neon, 2.0)
		"toybox":
			if variant % 2 == 0: # a stack of toy blocks
				var y := 0.0
				for k in randi_range(3, 6):
					var block := randf_range(0.7, 1.3)
					P.box(root, Vector3.ONE * block, at + Vector3(randf_range(-0.15, 0.15), y + block * 0.5, randf_range(-0.15, 0.15)), PRIMARY[(variant + k) % PRIMARY.size()], Vector3(0, randf() * TAU, 0))
					y += block
			else: # a giant crayon standing on end
				var color: Color = PRIMARY[variant % PRIMARY.size()]
				_cylinder(root, 0.35, 0.35, height * 0.7, at + Vector3.UP * height * 0.35, color)
				_cylinder(root, 0.37, 0.37, height * 0.25, at + Vector3.UP * height * 0.35, Color(1, 1, 1))
				P.cone(root, 0.35, 0.8, at + Vector3.UP * (height * 0.7 + 0.4), color)
		"skyislands": # floating rocks with a little round tree
			var lift := height * 0.3 + randf_range(0.5, 3.0)
			var rock := P.cone(root, height * 0.25, height * 0.4, at + Vector3.UP * (lift - height * 0.2), Color(0.55, 0.45, 0.4))
			rock.rotation.x = PI
			_cylinder(root, height * 0.25, height * 0.25, 0.2, at + Vector3.UP * (lift + 0.1), Color(0.45, 0.72, 0.35))
			_cylinder(root, 0.08, 0.12, height * 0.25, at + Vector3.UP * (lift + height * 0.12), Color(0.4, 0.28, 0.2))
			_sphere(root, height * 0.12, at + Vector3.UP * (lift + height * 0.3), Color(0.4, 0.68, 0.32))
		"jungle": # tall canopy trees and big leaves
			_cylinder(root, 0.2, 0.35, height * 1.2, at + Vector3.UP * height * 0.6, Color(0.35, 0.3, 0.22))
			for k in 3:
				_sphere(root, height * randf_range(0.25, 0.35), at + Vector3(randf_range(-1, 1), height * randf_range(1.1, 1.3), randf_range(-1, 1)), [Color(0.15, 0.4, 0.15), Color(0.25, 0.52, 0.18)][k % 2], Vector3(1, 0.45, 1))
			for k in 4: # big leaves at the foot
				P.box(root, Vector3(0.6, 0.03, 1.4), at + Vector3(randf_range(-0.8, 0.8), 0.4, randf_range(-0.8, 0.8)), Color(0.2, 0.5, 0.15), Vector3(randf_range(-0.6, -0.3), randf() * TAU, 0))
		"cloud":
			if variant % 3 == 0: # a marble column
				_cylinder(root, 0.4, 0.4, height, at + Vector3.UP * height * 0.5, Color(0.97, 0.96, 0.93))
				P.box(root, Vector3(1.0, 0.25, 1.0), at + Vector3.UP * (height + 0.12), Color(0.95, 0.85, 0.55))
			else: # a cloud puff hanging in the air
				var lift := height * randf_range(0.3, 0.9)
				for k in 4:
					_sphere(root, height * randf_range(0.12, 0.2), at + Vector3(randf_range(-1.2, 1.2), lift + randf_range(-0.3, 0.3), randf_range(-1.2, 1.2)), Color(1, 1, 1))
		"toxic":
			if variant % 3 == 0: # rusty pipes
				_cylinder(root, 0.3, 0.3, height, at + Vector3.UP * height * 0.5, Color(0.45, 0.28, 0.18))
				_cylinder(root, 0.25, 0.25, height * 0.6, at + Vector3(height * 0.3, height * 0.8, 0), Color(0.45, 0.28, 0.18)).rotation.z = PI * 0.5
			else: # dead grey stump
				var stump := P.dead_tree(root, at, height * 0.5)
				_recolor_all(stump, Color(0.3, 0.3, 0.28))
		"magical":
			if variant % 3 == 0: # crystal cluster
				for k in 4:
					var crystal := P.box(root, Vector3(0.3, height * randf_range(0.2, 0.4), 0.3), at + Vector3(randf_range(-0.5, 0.5), height * 0.12, randf_range(-0.5, 0.5)), Color.WHITE, Vector3(randf_range(-0.4, 0.4), randf() * TAU, randf_range(-0.4, 0.4)))
					crystal.material_override = P.material([Color(0.4, 0.9, 1.0), Color(0.8, 0.5, 1.0)][variant % 2], 1.8)
			else: # giant mushroom with a glowing underside
				_cylinder(root, 0.2, 0.3, height * 0.7, at + Vector3.UP * height * 0.35, Color(0.8, 0.8, 0.88))
				var cap: Color = [Color(0.5, 0.2, 0.7), Color(0.15, 0.45, 0.6), Color(0.7, 0.2, 0.45)][variant % 3]
				_sphere(root, height * 0.35, at + Vector3.UP * height * 0.75, cap, Vector3(1, 0.45, 1))
				var under := _sphere(root, height * 0.3, at + Vector3.UP * height * 0.68, cap, Vector3(1, 0.2, 1))
				under.material_override = P.material(Color(0.4, 1.0, 0.9), 1.5)


# --- landmarks ------------------------------------------------------------------------------------

static func _landmark(kit: String, root: Node3D, at: Vector3, height: float, index: int, look: MoodLook) -> void:
	var top := height
	match kit:
		"candy": # a layered cake with a cherry
			var y := 0.0
			for k in 4:
				var r := 2.4 - k * 0.5
				var tier := 1.6
				var color: Color = [Color(1.0, 0.75, 0.85), Color(0.75, 0.9, 1.0), Color(1.0, 0.95, 0.7), Color(0.85, 1.0, 0.8)][k]
				_cylinder(root, r, r, tier, at + Vector3.UP * (y + tier * 0.5), color)
				_torus(root, r * 0.95, r * 1.05, at + Vector3.UP * (y + tier), Color(1, 1, 1))
				y += tier
			_sphere(root, 0.5, at + Vector3.UP * (y + 0.4), Color(0.9, 0.1, 0.2)).material_override = P.material(Color(0.9, 0.1, 0.2), 0.0, 0.7)
			top = y + 0.9
		"desert": # a mesa with stripes of sandstone
			var y := 0.0
			var k := 0
			while y < height:
				var band := randf_range(0.8, 1.6)
				var w := lerpf(5.5, 3.5, y / height) + randf_range(-0.3, 0.3)
				P.box(root, Vector3(w, band, w * 0.8), at + Vector3.UP * (y + band * 0.5), [Color(0.82, 0.55, 0.38), Color(0.72, 0.42, 0.3), Color(0.9, 0.68, 0.48)][k % 3], Vector3(0, randf_range(-0.1, 0.1), 0))
				y += band
				k += 1
			top = y
		"autumn": # a red barn tower with a roof
			P.box(root, Vector3(3.0, height * 0.6, 3.0), at + Vector3.UP * height * 0.3, Color(0.65, 0.18, 0.12))
			var roof := P.cone(root, 2.6, height * 0.3, at + Vector3.UP * height * 0.75, Color(0.3, 0.22, 0.18))
			roof.scale = Vector3(1, 1, 1)
			top = height * 0.9
		"cyberpunk": # a skyscraper with a billboard and an antenna light
			var tall := height * 3.0
			P.box(root, Vector3(4.0, tall, 4.0), at + Vector3.UP * tall * 0.5, Color(0.08, 0.08, 0.1))
			var board := P.box(root, Vector3(3.0, 2.0, 0.1), at + Vector3(0, tall * 0.6, -2.1), Color.WHITE)
			board.material_override = P.material([Color(1.0, 0.2, 0.8), Color(0.2, 0.9, 1.0)][index % 2], 2.5)
			top = tall
		"solarpunk": # a white terraced tower with gardens on every level
			var y := 0.0
			for k in 4:
				var r := 2.2 - k * 0.35
				var level := height / 4.0
				_cylinder(root, r, r * 0.9, level, at + Vector3.UP * (y + level * 0.5), Color(0.95, 0.95, 0.92))
				_cylinder(root, r * 1.05, r * 1.05, 0.25, at + Vector3.UP * (y + level), Color(0.35, 0.65, 0.3))
				y += level
			top = y
		"medieval": # a castle tower with a cone roof and a flag
			_cylinder(root, 1.8, 2.0, height, at + Vector3.UP * height * 0.5, Color(0.55, 0.53, 0.5))
			P.cone(root, 2.2, 3.0, at + Vector3.UP * (height + 1.5), Color(0.7, 0.15, 0.12))
			P.box(root, Vector3(0.06, 1.6, 0.06), at + Vector3.UP * (height + 3.6), P.POST)
			MoodShaders.flag(root, at + Vector3(0, height + 4.1, 0), Vector2(1.4, 0.8), Color(0.15, 0.3, 0.75))
			top = height
		"toybox": # a giant tower of building blocks
			var y := 0.0
			var k := 0
			while y < height:
				var block := randf_range(1.6, 2.4)
				P.box(root, Vector3.ONE * block, at + Vector3(randf_range(-0.2, 0.2), y + block * 0.5, 0), PRIMARY[(index + k) % PRIMARY.size()], Vector3(0, randf_range(-0.3, 0.3), 0))
				y += block
				k += 1
			top = y
		"skyislands": # a big floating island with a cottage and a waterfall
			var lift := height * 0.5
			var rock := P.cone(root, 4.0, 5.0, at + Vector3.UP * (lift - 2.5), Color(0.55, 0.45, 0.4))
			rock.rotation.x = PI
			_cylinder(root, 4.0, 4.0, 0.4, at + Vector3.UP * (lift + 0.2), Color(0.45, 0.72, 0.35))
			P.box(root, Vector3(1.8, 1.4, 1.6), at + Vector3(0.8, lift + 1.1, 0), Color(0.95, 0.9, 0.8))
			P.cone(root, 1.5, 1.2, at + Vector3(0.8, lift + 2.4, 0), Color(0.8, 0.3, 0.25))
			P.box(root, Vector3(0.05, 1.4, 0.05), at + Vector3(0.8, lift + 3.6, 0), P.POST)
			MoodShaders.flag(root, at + Vector3(0.8, lift + 4.0, 0), Vector2(0.8, 0.45), Color(0.95, 0.85, 0.3))
			var fall := P.box(root, Vector3(0.8, lift + 6.0, 0.1), at + Vector3(-2.5, lift * 0.5 - 3.0, 2.0), Color(0.7, 0.88, 1.0))
			fall.material_override = P.material(Color(0.7, 0.88, 1.0), 0.6)
			top = lift + 0.4
		"jungle": # a step pyramid, mossy, a glowing doorway at the top
			var y := 0.0
			for k in 5:
				var w := 7.0 - k * 1.3
				P.box(root, Vector3(w, 1.4, w), at + Vector3.UP * (y + 0.7), Color(0.5, 0.52, 0.45) if k % 2 == 0 else Color(0.42, 0.5, 0.36))
				y += 1.4
			P.box(root, Vector3(1.6, 1.6, 1.6), at + Vector3.UP * (y + 0.8), Color(0.45, 0.47, 0.4))
			P.box(root, Vector3(0.6, 0.9, 0.05), at + Vector3(0, y + 0.6, -0.81), Color(0.4, 1.0, 0.5)).material_override = P.material(Color(0.4, 1.0, 0.5), 3.0)
			top = y + 1.6
		"cloud":
			if index == 0: # a rainbow, half sunk into the clouds
				var bands := [Color(1, 0.3, 0.3), Color(1, 0.6, 0.2), Color(1, 0.95, 0.3), Color(0.4, 0.9, 0.4), Color(0.3, 0.6, 1), Color(0.6, 0.4, 1)]
				for k in bands.size():
					var r := 9.0 - k * 0.5
					var band := _torus(root, r - 0.5, r, at, bands[k], Vector3(PI * 0.5, 0, 0))
					band.material_override = P.material(bands[k], 0.8)
				return
			for k in 8: # a ring of columns under a golden dome
				var angle := TAU * k / 8.0
				_cylinder(root, 0.35, 0.35, height * 0.6, at + Vector3(cos(angle) * 3.0, height * 0.3, sin(angle) * 3.0), Color(0.97, 0.96, 0.93))
			_sphere(root, 3.6, at + Vector3.UP * height * 0.62, Color(0.95, 0.8, 0.4), Vector3(1, 0.55, 1)).material_override = P.material(Color(0.95, 0.8, 0.4), 0.0, 0.7)
			top = height * 0.62 + 2.0
		"toxic": # a cooling tower: smoke-grey, a red warning light, ooze at its foot
			_cylinder(root, 3.0, 2.0, height * 1.2, at + Vector3.UP * height * 0.6, Color(0.5, 0.5, 0.48))
			_torus(root, 1.9, 2.1, at + Vector3.UP * height * 1.2, Color(1.0, 0.1, 0.05), Vector3.ZERO).material_override = P.material(Color(1.0, 0.1, 0.05), 3.0)
			_cylinder(root, 4.5, 4.5, 0.03, at + Vector3.UP * 0.02, Color(0.6, 1.0, 0.2)).material_override = P.material(Color(0.6, 1.0, 0.2), 2.0)
			top = height * 1.2
		"magical": # a floating island: an upside-down rock cone with a grassy top
			var float_y := height * 0.6
			var rock := P.cone(root, 3.0, 4.0, at + Vector3.UP * (float_y - 2.0), Color(0.3, 0.3, 0.4))
			rock.rotation.x = PI
			_cylinder(root, 3.0, 3.0, 0.4, at + Vector3.UP * (float_y + 0.2), Color(0.2, 0.45, 0.4))
			top = float_y + 0.4
		_: # spooky: a stone tower
			var tower := P.tower(root, at, height)
			top = (tower.get_child(-1) as Node3D).position.y
	if not kit in ["cyberpunk", "candy", "toxic"]:
		_lamp(kit, root, at + Vector3.UP * top, 0.0, index, look)


# --- shape helpers ---------------------------------------------------------------------------------

static func _root(parent: Node3D, at: Vector3, yaw: float) -> Node3D:
	var root := Node3D.new()
	root.position = at
	root.rotation.y = yaw
	parent.add_child(root)
	return root


static func _sphere(parent: Node3D, radius: float, at: Vector3, color: Color, scale := Vector3.ONE) -> MeshInstance3D:
	var shape := SphereMesh.new()
	shape.radius = radius
	shape.height = radius * 2.0
	shape.radial_segments = 12
	shape.rings = 6
	var instance := P.mesh(parent, shape, at, color)
	instance.scale = scale
	return instance


static func _cylinder(parent: Node3D, bottom: float, top: float, height: float, at: Vector3, color: Color) -> MeshInstance3D:
	var shape := CylinderMesh.new()
	shape.bottom_radius = bottom
	shape.top_radius = top
	shape.height = height
	shape.radial_segments = 12
	return P.mesh(parent, shape, at, color)


static func _capsule(parent: Node3D, radius: float, height: float, at: Vector3, color: Color, rotation := Vector3.ZERO) -> MeshInstance3D:
	var shape := CapsuleMesh.new()
	shape.radius = radius
	shape.height = maxf(height, radius * 2.0)
	return P.mesh(parent, shape, at, color, rotation)


static func _torus(parent: Node3D, inner: float, outer: float, at: Vector3, color: Color, rotation := Vector3.ZERO) -> MeshInstance3D:
	var shape := TorusMesh.new()
	shape.inner_radius = inner
	shape.outer_radius = outer
	return P.mesh(parent, shape, at, color, rotation)


static func _recolor_all(node: Node, color: Color) -> void:
	for child in node.get_children():
		if child is MeshInstance3D:
			(child as MeshInstance3D).material_override = P.material(color)
		_recolor_all(child, color)


static func _recolor(node: Node, color: Color) -> void:
	for child in node.get_children():
		if child is MeshInstance3D and (child as MeshInstance3D).material_override == P.material(P.PINE):
			(child as MeshInstance3D).material_override = P.material(color)
		_recolor(child, color)
