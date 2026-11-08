# Bakes the slime shader's two textures in Blender (Cycles, Emit bake), from plain node materials
# you can open and read: slime/slime_bake.blend (one frame per channel, left to right).
#
#   blender -b -P tools/bake_slime.py -- <liquid folder>      (make slime-bake, from ../)
#
# Out, into <liquid folder>/slime/:
#   slime_pattern.png   R  height    blobby pillows, low in the valleys between them
#                       G  veins     thin wiggly lines along the valleys (they glow)
#                       B  bubbles   1 at a bubble's centre falling to 0 at its rim (the shader
#                                    thresholds it: a bubble grows as the threshold drops)
#                       A  phase     a random number per bubble: when in its cycle it is born
#   slime_slopes.png    RG height's slope (x, y), for the surface normal; 0.5 = flat
#                       BA the way out of the bubble from its centre (x, y), for its dome; 0.5 = none
#   slime_bake.blend    the node materials (open it to see how each channel is made)
#   slime_channels.png  the four channels side by side (to look at)
#
# Seamless: every pattern is 4D noise / Voronoi read on a torus. The texture's u and v each go
# once round a circle (cos, sin), so (cos u, sin u, cos v, sin v) * R comes back to where it
# started at both edges, and the pattern with it. That torus is flat (its 2D distances are kept),
# so the cells aren't stretched.
#
# The slopes and the bubble directions are worked out here with numpy from the baked channels
# (wrapping round the edges), not baked: they need the neighbouring pixels.

import math
import os
import sys

import bpy
import numpy as np

SIZE = 1024 # pixels a side
SAMPLES = 8 # Cycles samples per pixel: smooths the thin veins' edges
SLOPE_RANGE = 40.0 # steepest slope kept (height 0..1 per tile); stored as sqrt (see below)
RING = 1.0 # R: the torus' radius; the pattern repeats once round, so cells per tile ~ 2 pi R x scale

# Pattern knobs (Voronoi scale: cells about this far apart, in torus units x scale)
BLOB_SCALE = 0.85 # ~5 pillows across the tile
WARP = 0.35 # how much the noise bends the cells (wiggly veins)
VEIN_WIDTH = 0.045 # edge distance that still glows
PILLOW = 0.4 # edge distance where a pillow reaches its top
BIG_SCALE = 1.1 # big bubbles: ~7 cells across
BIG_RADIUS = 0.5 # of a cell (in 4D: the slice through it is smaller)
BIG_CHANCE = 0.8 # of the cells have one
SMALL_SCALE = 2.6 # small bubbles: ~16 across
SMALL_RADIUS = 0.45
SMALL_CHANCE = 0.8


# ---------------------------------------------------------------- node helpers

def node(p_nodes, p_type, p_x, p_y, p_frame=None, p_label=None, **p_props):
	n = p_nodes.new(p_type)
	n.location = (p_x, p_y)
	if p_frame:
		n.parent = p_frame
	if p_label:
		n.label = p_label
	for key, value in p_props.items():
		setattr(n, key, value)
	return n


def math_node(p_nodes, p_op, p_x, p_y, p_frame, p_b=None, p_label=None, p_clamp=False):
	n = node(p_nodes, "ShaderNodeMath", p_x, p_y, p_frame, p_label, operation=p_op, use_clamp=p_clamp)
	if p_b is not None:
		n.inputs[1].default_value = p_b
	return n


def frame(p_nodes, p_label, p_colour):
	f = p_nodes.new("NodeFrame")
	f.label = p_label
	f.use_custom_color = True
	f.color = p_colour
	f.label_size = 28
	return f


# ---------------------------------------------------------------- the material

# Builds the slime pattern material. Returns the sockets of the four channels (each 0..1).
def build_material():
	mat = bpy.data.materials.new("slime_bake")
	nt = mat.node_tree
	nodes, links = nt.nodes, nt.links
	nodes.clear()
	link = links.new

	# 1  the torus: texture u, v -> (R cos 2pi u, R sin 2pi u, R cos 2pi v) and W = R sin 2pi v
	f1 = frame(nodes, "1  Torus: u, v round two circles (seamless)", (0.25, 0.3, 0.45))
	coord = node(nodes, "ShaderNodeTexCoord", -2000, 0, f1)
	sep = node(nodes, "ShaderNodeSeparateXYZ", -1800, 0, f1)
	link(coord.outputs["UV"], sep.inputs[0])
	angle_u = math_node(nodes, "MULTIPLY", -1600, 120, f1, 2 * math.pi, "u x 2 pi")
	angle_v = math_node(nodes, "MULTIPLY", -1600, -120, f1, 2 * math.pi, "v x 2 pi")
	link(sep.outputs["X"], angle_u.inputs[0])
	link(sep.outputs["Y"], angle_v.inputs[0])
	parts = []
	for i, (angle, op) in enumerate([(angle_u, "COSINE"), (angle_u, "SINE"), (angle_v, "COSINE"), (angle_v, "SINE")]):
		trig = math_node(nodes, op, -1400, 240 - i * 160, f1, None, op.lower())
		link(angle.outputs[0], trig.inputs[0])
		scaled = math_node(nodes, "MULTIPLY", -1220, 240 - i * 160, f1, RING, "x R")
		link(trig.outputs[0], scaled.inputs[0])
		parts.append(scaled)
	torus_xyz = node(nodes, "ShaderNodeCombineXYZ", -1020, 160, f1, "xyz")
	for i in range(3):
		link(parts[i].outputs[0], torus_xyz.inputs[i])
	torus_w = parts[3] # W

	# 2  warp: 4D noise on the same torus pushes the cells about, so their edges wiggle
	f2 = frame(nodes, "2  Warp: noise bends the cells", (0.35, 0.3, 0.2))
	warp_noise = node(nodes, "ShaderNodeTexNoise", -800, 300, f2, "noise 4D", noise_dimensions="4D")
	warp_noise.inputs["Scale"].default_value = 1.6
	warp_noise.inputs["Detail"].default_value = 3.0
	link(torus_xyz.outputs[0], warp_noise.inputs["Vector"])
	link(torus_w.outputs[0], warp_noise.inputs["W"])
	centred = node(nodes, "ShaderNodeVectorMath", -600, 300, f2, "- 0.5", operation="SUBTRACT")
	centred.inputs[1].default_value = (0.5, 0.5, 0.5)
	link(warp_noise.outputs["Color"], centred.inputs[0])
	push = node(nodes, "ShaderNodeVectorMath", -420, 300, f2, "x warp", operation="SCALE")
	push.inputs["Scale"].default_value = WARP
	link(centred.outputs[0], push.inputs[0])
	warped_xyz = node(nodes, "ShaderNodeVectorMath", -240, 300, f2, "xyz + push", operation="ADD")
	link(torus_xyz.outputs[0], warped_xyz.inputs[0])
	link(push.outputs[0], warped_xyz.inputs[1])
	w_push = math_node(nodes, "SUBTRACT", -600, 120, f2, 0.5, "fac - 0.5")
	link(warp_noise.outputs["Fac"], w_push.inputs[0])
	w_scaled = math_node(nodes, "MULTIPLY", -420, 120, f2, WARP, "x warp")
	link(w_push.outputs[0], w_scaled.inputs[0])
	warped_w = math_node(nodes, "ADD", -240, 120, f2, None, "w + push")
	link(torus_w.outputs[0], warped_w.inputs[0])
	link(w_scaled.outputs[0], warped_w.inputs[1])

	# 3  cells: how far each point is from the nearest cell edge (0 on an edge, ~0.5 at a centre)
	f3 = frame(nodes, "3  Cells: distance to the nearest edge", (0.2, 0.35, 0.25))
	edges = node(nodes, "ShaderNodeTexVoronoi", 0, 300, f3, "voronoi 4D, distance to edge",
			voronoi_dimensions="4D", feature="DISTANCE_TO_EDGE")
	edges.inputs["Scale"].default_value = BLOB_SCALE
	link(warped_xyz.outputs[0], edges.inputs["Vector"])
	link(warped_w.outputs[0], edges.inputs["W"])

	# 4  R height: a pillow per cell, plus fine lumps. x = the edge distance 0..PILLOW as 0..1;
	# x (2 - x) rises steeply out of the vein and rounds off on top (a dome, not a plateau)
	f4 = frame(nodes, "4  R  Height: pillows + lumps", (0.4, 0.2, 0.2))
	rise = node(nodes, "ShaderNodeMapRange", 150, 480, f4, "edge 0..PILLOW -> x 0..1")
	rise.inputs["From Max"].default_value = PILLOW
	link(edges.outputs["Distance"], rise.inputs["Value"])
	two_minus = math_node(nodes, "SUBTRACT", 330, 540, f4, None, "2 - x")
	two_minus.inputs[0].default_value = 2.0
	link(rise.outputs["Result"], two_minus.inputs[1])
	pillow = math_node(nodes, "MULTIPLY", 330, 400, f4, None, "x (2 - x): dome")
	link(rise.outputs["Result"], pillow.inputs[0])
	link(two_minus.outputs[0], pillow.inputs[1])
	lumps = node(nodes, "ShaderNodeTexNoise", 250, 220, f4, "lumps 4D", noise_dimensions="4D")
	lumps.inputs["Scale"].default_value = 6.0
	lumps.inputs["Detail"].default_value = 2.0
	link(torus_xyz.outputs[0], lumps.inputs["Vector"])
	link(torus_w.outputs[0], lumps.inputs["W"])
	lump_part = math_node(nodes, "MULTIPLY", 450, 220, f4, 0.18, "x 0.18")
	link(lumps.outputs["Fac"], lump_part.inputs[0])
	pillow_part = math_node(nodes, "MULTIPLY", 450, 420, f4, 0.9, "x 0.9")
	link(pillow.outputs[0], pillow_part.inputs[0])
	height = math_node(nodes, "ADD", 650, 360, f4, None, "R height", True)
	link(pillow_part.outputs[0], height.inputs[0])
	link(lump_part.outputs[0], height.inputs[1])

	# 5  G veins: 1 on an edge fading out by VEIN_WIDTH; slow noise makes some veins thicker
	f5 = frame(nodes, "5  G  Veins: the edges, some thicker", (0.35, 0.35, 0.1))
	thick = node(nodes, "ShaderNodeTexNoise", 250, 0, f5, "thickness 4D", noise_dimensions="4D")
	thick.inputs["Scale"].default_value = 1.2
	link(torus_xyz.outputs[0], thick.inputs["Vector"])
	link(torus_w.outputs[0], thick.inputs["W"])
	width = node(nodes, "ShaderNodeMapRange", 450, 0, f5, "noise -> width")
	width.inputs["From Min"].default_value = 0.35
	width.inputs["From Max"].default_value = 0.65
	width.inputs["To Min"].default_value = VEIN_WIDTH * 0.35
	width.inputs["To Max"].default_value = VEIN_WIDTH * 1.6
	link(thick.outputs["Fac"], width.inputs["Value"])
	vein_ramp = node(nodes, "ShaderNodeMapRange", 650, 120, f5, "edge 0..width -> 1..0",
			interpolation_type="SMOOTHSTEP")
	vein_ramp.inputs["To Min"].default_value = 1.0
	vein_ramp.inputs["To Max"].default_value = 0.0
	link(edges.outputs["Distance"], vein_ramp.inputs["Value"])
	link(width.outputs["Result"], vein_ramp.inputs["From Max"])

	# 6, 7  B bubbles and A phase: two sizes. Each Voronoi cell may hold one bubble: its random
	# colour says whether (G < chance), how big (R) and when it is born (B: the phase).
	bubbles = []
	for k, (label, scale, radius, chance, y, colour) in enumerate([
			("6  B  Big bubbles", BIG_SCALE, BIG_RADIUS, BIG_CHANCE, -320, (0.15, 0.3, 0.4)),
			("7  B  Small bubbles", SMALL_SCALE, SMALL_RADIUS, SMALL_CHANCE, -720, (0.15, 0.25, 0.45))]):
		f = frame(nodes, label + ": 1 at the centre .. 0 at the rim", colour)
		cell = node(nodes, "ShaderNodeTexVoronoi", 0, y, f, "voronoi 4D, F1", voronoi_dimensions="4D",
				feature="F1")
		cell.inputs["Scale"].default_value = scale
		cell.inputs["Randomness"].default_value = 0.75 # not too close together
		link(torus_xyz.outputs[0], cell.inputs["Vector"])
		link(torus_w.outputs[0], cell.inputs["W"])
		rand = node(nodes, "ShaderNodeSeparateColor", 220, y - 120, f, "cell's random colour")
		link(cell.outputs["Color"], rand.inputs[0])
		size = node(nodes, "ShaderNodeMapRange", 420, y - 60, f, "R -> radius")
		size.inputs["To Min"].default_value = radius * 0.55
		size.inputs["To Max"].default_value = radius
		link(rand.outputs["Red"], size.inputs["Value"])
		ratio = math_node(nodes, "DIVIDE", 620, y + 40, f, None, "distance / radius")
		link(cell.outputs["Distance"], ratio.inputs[0])
		link(size.outputs["Result"], ratio.inputs[1])
		dome = math_node(nodes, "SUBTRACT", 800, y + 40, f, None, "1 - that", True)
		dome.inputs[0].default_value = 1.0
		link(ratio.outputs[0], dome.inputs[1])
		has = math_node(nodes, "LESS_THAN", 620, y - 160, f, chance, "G < chance: has one")
		link(rand.outputs["Green"], has.inputs[0])
		bubble = math_node(nodes, "MULTIPLY", 980, y, f, None, "bubble")
		link(dome.outputs[0], bubble.inputs[0])
		link(has.outputs[0], bubble.inputs[1])
		bubbles.append((bubble, rand))

	# 8  combine: the bigger value wins, and its cell's phase with it (phase kept off 0: Godot's
	# import would otherwise repaint the colour of fully transparent pixels)
	f8 = frame(nodes, "8  B = max of both, A = the winner's phase", (0.3, 0.3, 0.3))
	(big, big_rand), (small, small_rand) = bubbles
	both = math_node(nodes, "MAXIMUM", 1200, -480, f8, None, "B bubbles")
	link(big.outputs[0], both.inputs[0])
	link(small.outputs[0], both.inputs[1])
	big_wins = math_node(nodes, "GREATER_THAN", 1200, -680, f8, None, "big > small")
	link(big.outputs[0], big_wins.inputs[0])
	link(small.outputs[0], big_wins.inputs[1])
	pick = node(nodes, "ShaderNodeMix", 1400, -680, f8, "winner's B (phase)", data_type="FLOAT")
	link(big_wins.outputs[0], pick.inputs["Factor"])
	link(small_rand.outputs["Blue"], pick.inputs["A"])
	link(big_rand.outputs["Blue"], pick.inputs["B"])
	phase = node(nodes, "ShaderNodeMapRange", 1600, -680, f8, "0..1 -> 0.03..1 (A phase)")
	phase.inputs["To Min"].default_value = 0.03
	link(pick.outputs["Result"], phase.inputs["Value"])

	# 9  bake: the emission shows whichever channel is being baked (the script relinks it)
	f9 = frame(nodes, "9  Bake: Emit, one channel at a time", (0.25, 0.25, 0.25))
	emit = node(nodes, "ShaderNodeEmission", 1900, 0, f9, "channel")
	out = node(nodes, "ShaderNodeOutputMaterial", 2100, 0, f9)
	link(emit.outputs[0], out.inputs["Surface"])
	target = node(nodes, "ShaderNodeTexImage", 1900, -250, f9, "bake target")

	channels = {
		"height": height.outputs[0],
		"veins": vein_ramp.outputs["Result"],
		"bubbles": both.outputs[0],
		"phase": phase.outputs["Result"],
	}
	return mat, emit, target, channels


# ---------------------------------------------------------------- baking

def bake_channel(p_mat, p_emit, p_target, p_socket, p_image):
	nt = p_mat.node_tree
	nt.links.new(p_socket, p_emit.inputs["Color"])
	p_target.image = p_image
	nt.nodes.active = p_target
	bpy.ops.object.bake(type="EMIT")
	pixels = np.empty(SIZE * SIZE * 4, dtype=np.float32)
	p_image.pixels.foreach_get(pixels)
	# rows bottom-up in Blender; flip so row 0 is the top, as in the PNG and in Godot's UV
	return np.flipud(pixels.reshape(SIZE, SIZE, 4)[:, :, 0])


# Slope with wrap-around (the texture tiles): d/dx along a row, d/dy down the rows (Godot's UV).
def wrapped_slope(p_value):
	dx = (np.roll(p_value, -1, axis=1) - np.roll(p_value, 1, axis=1)) * 0.5
	dy = (np.roll(p_value, -1, axis=0) - np.roll(p_value, 1, axis=0)) * 0.5
	return dx, dy


def save_rgba(p_path, p_rgba):
	image = bpy.data.images.new(os.path.basename(p_path), SIZE, SIZE, alpha=True, float_buffer=False)
	image.colorspace_settings.name = "Non-Color" # data, not colour: saved as is
	image.alpha_mode = "STRAIGHT"
	image.pixels.foreach_set(np.flipud(np.clip(p_rgba, 0.0, 1.0)).astype(np.float32).ravel())
	image.filepath_raw = p_path
	image.file_format = "PNG"
	image.save()
	print("saved", p_path)


def main():
	argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
	liquid = os.path.abspath(argv[0] if argv else os.path.join(os.path.dirname(__file__), ".."))
	out_dir = os.path.join(liquid, "slime")
	os.makedirs(out_dir, exist_ok=True)

	bpy.ops.wm.read_factory_settings(use_empty=True)
	scene = bpy.context.scene
	scene.render.engine = "CYCLES"
	scene.cycles.device = "CPU"
	scene.cycles.samples = SAMPLES
	bpy.ops.mesh.primitive_plane_add(size=2.0)
	plane = bpy.context.active_object
	plane.name = "bake_plane"
	mat, emit, target, channels = build_material()
	plane.data.materials.append(mat)

	baked = {}
	for name, socket in channels.items():
		image = bpy.data.images.new("bake_" + name, SIZE, SIZE, float_buffer=True)
		image.colorspace_settings.name = "Non-Color"
		baked[name] = bake_channel(mat, emit, target, socket, image)
		print("baked", name, float(baked[name].min()), float(baked[name].max()))
	nt = mat.node_tree
	nt.links.new(channels["height"], emit.inputs["Color"]) # the .blend opens showing the height

	h, v, b, ph = baked["height"], baked["veins"], baked["bubbles"], baked["phase"]
	save_rgba(os.path.join(out_dir, "slime_pattern.png"), np.dstack([h, v, b, ph]))

	# Height slope, in height (0..1) per tile (one pixel is 1 / SIZE of the tile): gentle on the
	# pillows (~1), steep into the veins (~30). Stored as sign * sqrt(|slope| / SLOPE_RANGE) round
	# 0.5, so the gentle slopes keep their 8 bits; the shader squares it back (e * |e| * range).
	hx, hy = wrapped_slope(h)
	hx, hy = hx * SIZE / SLOPE_RANGE, hy * SIZE / SLOPE_RANGE
	print("height slope range (x SLOPE_RANGE)", float(min(hx.min(), hy.min())), float(max(hx.max(), hy.max())))
	hx = np.sign(hx) * np.sqrt(np.minimum(np.abs(hx), 1.0))
	hy = np.sign(hy) * np.sqrt(np.minimum(np.abs(hy), 1.0))
	# The way out of each bubble: against B's slope (B falls from the centre), as a unit vector.
	bx, by = wrapped_slope(b)
	length = np.sqrt(bx * bx + by * by) + 1e-6
	inside = (b > 0.0).astype(np.float32)
	ox, oy = -bx / length * inside, -by / length * inside
	save_rgba(os.path.join(out_dir, "slime_slopes.png"),
			np.dstack([hx * 0.5 + 0.5, hy * 0.5 + 0.5, ox * 0.5 + 0.5, oy * 0.5 + 0.5]))

	# The four channels side by side, to look at.
	sheet = np.ones((SIZE // 2, SIZE * 2, 4), dtype=np.float32)
	for i, channel in enumerate([h, v, b, ph * (b > 0)]):
		small = channel[::2, ::2]
		sheet[:, i * SIZE // 2:(i + 1) * SIZE // 2, 0] = small
		sheet[:, i * SIZE // 2:(i + 1) * SIZE // 2, 1] = small
		sheet[:, i * SIZE // 2:(i + 1) * SIZE // 2, 2] = small
	preview = bpy.data.images.new("slime_channels", SIZE * 2, SIZE // 2, alpha=True, float_buffer=False)
	preview.colorspace_settings.name = "Non-Color"
	preview.pixels.foreach_set(np.flipud(sheet).ravel())
	preview.filepath_raw = os.path.join(out_dir, "slime_channels.png")
	preview.file_format = "PNG"
	preview.save()

	for image in list(bpy.data.images):
		if image.name.startswith("bake_") or image.name == "slime_channels" or image.name.endswith(".png"):
			bpy.data.images.remove(image)
	bpy.context.preferences.filepaths.save_version = 0 # no .blend1 backup
	bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "slime_bake.blend"))
	print("saved", os.path.join(out_dir, "slime_bake.blend"))


main()
