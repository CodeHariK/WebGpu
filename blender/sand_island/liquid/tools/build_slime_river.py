# Builds the slime river sample in Blender: a winding channel running into a round pool, between
# stone banks, and the slime's surface, a dense flat mesh (0.1 m squares) carrying the same data
# as the minecraft game's MC water mesh, so every liquid shader reads it the same way.
#
#   blender -b -P tools/build_slime_river.py -- <liquid folder>      (make slime-river, from ../)
#
# Out, into <liquid folder>/sample/: slime_river.blend (to open and change) and slime_river.glb
# (the viewer loads it). Two objects: "banks" (stone) and "slime" (the surface).
#
# What the slime surface carries per vertex (as the MC water mesh, in Godot's axes):
#   COLOR  the flow: rg the direction (x, z) as 0..1 (0.5 = none), b 1 where it flows, a 0
#          river: along the channel, b = 1; pool: spreading out from where the river comes in,
#          fading (b falls to 0 away from the inlet)
#   UV     metres: world x, z (as the MC mesh's flat liquid)
# It has no UV2 or depth: the shaders read those as 0 (flat liquid, no falls).
#
# Blender is z up; glTF and Godot are y up: Blender (x, y, z) -> Godot (x, z, -y). So Godot's x, z
# are Blender's x, -y, and glTF flips v (v = 1 - Blender's v).

import math
import os
import sys

import bmesh
import bpy
import numpy as np

# The channel's centre line, Blender x, y (metres), through to the pool's centre.
CENTRE = [(-11.0, -5.0), (-7.5, -2.0), (-4.0, -4.5), (-0.5, -2.0), (1.5, 1.5), (4.0, 4.5), (6.0, 7.0)]
POOL_CENTRE = (6.0, 7.0)
POOL_RADIUS = 3.2
HALF_WIDTH = 1.1 # the channel: 2.2 m wide
WALL = 0.6 # the banks' thickness
BANK_TOP = 0.35 # metres above the slime
FLOOR = -0.5 # the channel's floor
SPACING = 0.1 # the slime surface's squares
INTO_WALL = 0.12 # the surface runs this far into the banks (its ragged edge hides in them)
POOL_SPREAD = 2.2 # metres from the inlet over which the pool's flow fades out


# ---------------------------------------------------------------- the centre line

# Catmull-Rom through CENTRE, sampled about every p_step metres: points (n, 2), tangents (n, 2).
def centre_line(p_step=0.15):
	pts = [np.array(p, dtype=float) for p in CENTRE]
	pts = [2 * pts[0] - pts[1]] + pts + [2 * pts[-1] - pts[-2]]
	out = []
	for i in range(1, len(pts) - 2):
		p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
		steps = max(int(np.linalg.norm(p2 - p1) / p_step), 1)
		for s in range(steps):
			t = s / steps
			out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
					+ (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
	out.append(pts[-2])
	points = np.array(out)
	tangents = np.gradient(points, axis=0)
	tangents /= np.linalg.norm(tangents, axis=1, keepdims=True)
	return points, tangents


# For each point of p_xy (m, 2): the distance to the centre line and the tangent of its nearest
# sample.
def nearest_on_line(p_xy, p_points, p_tangents):
	d = np.linalg.norm(p_xy[:, None, :] - p_points[None, :, :], axis=2)
	idx = np.argmin(d, axis=1)
	return d[np.arange(len(p_xy)), idx], p_tangents[idx]


# ---------------------------------------------------------------- the banks

# The outline of the channel + pool, p_half each side of the line and p_radius round the pool, as
# one closed loop (Blender x, y): up one side of the channel from its start, round the far side of
# the pool, back down the other side. p_from: the line sample the channel starts at.
def outline(p_points, p_tangents, p_half, p_radius, p_from):
	centre = np.array(POOL_CENTRE)
	sides = []
	for sign in (-1.0, 1.0):
		edge = []
		for p, t in zip(p_points[p_from:], p_tangents[p_from:]):
			q = p + sign * np.array([-t[1], t[0]]) * p_half
			if np.linalg.norm(q - centre) < p_radius:
				break # it has reached the pool
			edge.append(q)
		sides.append(edge)
	right, left = sides
	# round the pool, the long way: from where the right side meets it to where the left side does
	a_right = math.atan2(*(right[-1] - centre)[::-1])
	a_left = math.atan2(*(left[-1] - centre)[::-1])
	delta = math.atan2(math.sin(a_left - a_right), math.cos(a_left - a_right))
	delta -= math.copysign(2.0 * math.pi, delta)
	steps = max(int(abs(delta) * p_radius / 0.15), 8)
	arc = [centre + p_radius * np.array([math.cos(a_right + delta * k / steps), math.sin(a_right + delta * k / steps)])
			for k in range(1, steps)]
	loop = right + arc + left[::-1]
	xy = np.array(loop)
	area = 0.5 * np.sum(xy[:, 0] * np.roll(xy[:, 1], -1) - np.roll(xy[:, 0], -1) * xy[:, 1])
	return loop if area > 0.0 else loop[::-1] # counter-clockwise from above: inside on the left


# Stone banks, built straight from the two outlines (no booleans): the flat top between the outer
# outline (WALL out) and the inner one, the inner wall down to the channel's floor, the outer wall,
# and the floor. The channel's start is closed (the inner outline starts WALL further along).
def build_banks(p_points, p_tangents):
	start = int(WALL / 0.15) + 1
	inner = outline(p_points, p_tangents, HALF_WIDTH, POOL_RADIUS, start)
	outer = outline(p_points, p_tangents, HALF_WIDTH + WALL, POOL_RADIUS + WALL, 0)
	bm = bmesh.new()

	def loop(p_outline, p_z):
		return [bm.verts.new((q[0], q[1], p_z)) for q in p_outline]

	def wall(p_top, p_bottom, p_flip):
		for i in range(len(p_top)):
			j = (i + 1) % len(p_top)
			face = [p_top[i], p_top[j], p_bottom[j], p_bottom[i]]
			bm.faces.new(face[::-1] if p_flip else face)

	inner_top, outer_top = loop(inner, BANK_TOP), loop(outer, BANK_TOP)
	inner_floor, outer_foot = loop(inner, FLOOR), loop(outer, FLOOR - 0.1)
	wall(inner_top, inner_floor, False)
	wall(outer_top, outer_foot, True)
	# the top: fill between the two loops (the inner one is a hole)
	top_edges = []
	for ring in (inner_top, outer_top):
		for i in range(len(ring)):
			top_edges.append(bm.edges.get((ring[i], ring[(i + 1) % len(ring)])) or bm.edges.new((ring[i], ring[(i + 1) % len(ring)])))
	filled = bmesh.ops.triangle_fill(bm, edges=top_edges, use_beauty=True, use_dissolve=False)
	for face in filled["geom"]:
		if isinstance(face, bmesh.types.BMFace):
			face.normal_update()
			if face.normal.z < 0.0:
				face.normal_flip()
	bm.faces.new(inner_floor) # the channel's floor (counter-clockwise: facing up)
	# The walls' winding: the outlines run counter-clockwise, so the inside is on the left of each
	# edge; (top i, top j, bottom j, bottom i) then faces left: into the channel. The outer wall
	# is flipped: it faces out.
	banks = object_from_bmesh("banks", bm)
	stone = new_material("stone")
	stone.diffuse_color = (0.42, 0.38, 0.46, 1.0)
	bsdf = stone.node_tree.nodes.get("Principled BSDF")
	if bsdf:
		bsdf.inputs["Base Color"].default_value = (0.42, 0.38, 0.46, 1.0)
		bsdf.inputs["Roughness"].default_value = 0.9
	banks.data.materials.append(stone)
	return banks


# A node material (Blender 5 makes them with nodes; older versions need use_nodes switched on).
def new_material(p_name):
	mat = bpy.data.materials.new(p_name)
	if mat.node_tree is None:
		mat.use_nodes = True
	return mat


def object_from_bmesh(p_name, p_bm):
	mesh = bpy.data.meshes.new(p_name)
	p_bm.to_mesh(mesh)
	p_bm.free()
	obj = bpy.data.objects.new(p_name, mesh)
	bpy.context.scene.collection.objects.link(obj)
	return obj


# ---------------------------------------------------------------- the slime surface

def build_slime(p_points, p_tangents):
	xs = np.arange(-12.5, 10.0 + SPACING, SPACING)
	ys = np.arange(-8.0, 11.0 + SPACING, SPACING)
	gx, gy = np.meshgrid(xs, ys)
	xy = np.stack([gx.ravel(), gy.ravel()], axis=1)

	# how far inside the channel or pool (negative inside), the channel starting WALL in
	start = int(WALL / 0.15) + 1
	line_d, tangent = nearest_on_line(xy, p_points[start:], p_tangents[start:])
	pool_offset = xy - np.array(POOL_CENTRE)
	pool_d = np.linalg.norm(pool_offset, axis=1)
	sdf = np.minimum(line_d - HALF_WIDTH, pool_d - POOL_RADIUS)
	keep = (sdf < INTO_WALL).reshape(gx.shape)

	# a square is kept if all four corners are
	nx, ny = len(xs), len(ys)
	quad_keep = keep[:-1, :-1] & keep[1:, :-1] & keep[:-1, 1:] & keep[1:, 1:]
	used = np.zeros(gx.shape, dtype=bool)
	used[:-1, :-1] |= quad_keep
	used[1:, :-1] |= quad_keep
	used[:-1, 1:] |= quad_keep
	used[1:, 1:] |= quad_keep
	index = -np.ones(gx.shape, dtype=np.int64)
	index[used] = np.arange(used.sum())
	verts = [(x, y, 0.0) for x, y in xy[used.ravel()]]
	faces = []
	for j, i in zip(*np.nonzero(quad_keep)):
		faces.append((index[j, i], index[j, i + 1], index[j + 1, i + 1], index[j + 1, i]))

	# the flow, in Blender's x, y: the river along its line; the pool spreading out from the inlet
	vxy = xy[used.ravel()]
	v_line_d, v_tangent = nearest_on_line(vxy, p_points[start:], p_tangents[start:])
	v_pool = np.linalg.norm(vxy - np.array(POOL_CENTRE), axis=1)
	inlet_i = np.argmin(np.abs(np.linalg.norm(p_points - np.array(POOL_CENTRE), axis=1) - POOL_RADIUS))
	inlet, inlet_t = p_points[inlet_i], p_tangents[inlet_i]
	from_inlet = vxy - inlet
	inlet_dist = np.linalg.norm(from_inlet, axis=1, keepdims=True)
	radial = from_inlet / np.maximum(inlet_dist, 1e-4)
	spread = np.clip(inlet_dist / 1.5, 0.0, 1.0)
	pool_dir = inlet_t * (1.0 - spread) + radial * spread
	pool_dir /= np.maximum(np.linalg.norm(pool_dir, axis=1, keepdims=True), 1e-4)
	fade = np.exp(-inlet_dist[:, 0] / POOL_SPREAD)
	in_pool = np.clip((POOL_RADIUS - v_pool) / 0.6 + 0.5, 0.0, 1.0)[:, None] # blends over 0.6 m
	direction = v_tangent * (1.0 - in_pool) + pool_dir * fade[:, None] * in_pool
	flows = 1.0 - in_pool[:, 0] * (1.0 - fade)
	length = np.linalg.norm(direction, axis=1, keepdims=True)
	direction = np.where(length > 1.0, direction / np.maximum(length, 1e-4), direction)

	mesh = bpy.data.meshes.new("slime")
	mesh.from_pydata(verts, [], faces)
	mesh.update()
	obj = bpy.data.objects.new("slime", mesh)
	bpy.context.scene.collection.objects.link(obj)

	# COLOR: Godot's flow (x, z) = Blender's (x, -y), as 0..1
	colour = mesh.color_attributes.new("flow", "FLOAT_COLOR", "POINT")
	rgba = np.zeros((len(verts), 4), dtype=np.float32)
	rgba[:, 0] = direction[:, 0] * 0.5 + 0.5
	rgba[:, 1] = -direction[:, 1] * 0.5 + 0.5
	rgba[:, 2] = flows
	rgba[:, 3] = 0.0
	colour.data.foreach_set("color", rgba.ravel())
	mesh.color_attributes.active_color = colour
	mesh.color_attributes.render_color_index = 0

	# UV: Godot's world x, z in metres = Blender (x, -y); glTF flips v, so store 1 - (-y) = 1 + y
	uv_layer = mesh.uv_layers.new(name="UVMap")
	loop_vert = np.empty(len(mesh.loops), dtype=np.int64)
	mesh.loops.foreach_get("vertex_index", loop_vert)
	vco = np.array(verts)[:, :2]
	uvs = np.stack([vco[loop_vert, 0], 1.0 + vco[loop_vert, 1]], axis=1).astype(np.float32)
	uv_layer.data.foreach_set("uv", uvs.ravel())

	# a stand-in material (the viewer puts the liquid shaders on it): shows the flow colour
	mat = new_material("slime_flow")
	nt = mat.node_tree
	attr = nt.nodes.new("ShaderNodeVertexColor")
	attr.layer_name = "flow"
	nt.links.new(attr.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
	mesh.materials.append(mat)
	print("slime surface:", len(verts), "vertices,", len(faces), "squares")
	return obj


def main():
	argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
	liquid = os.path.abspath(argv[0] if argv else os.path.join(os.path.dirname(__file__), ".."))
	out_dir = os.path.join(liquid, "sample")
	os.makedirs(out_dir, exist_ok=True)
	bpy.ops.wm.read_factory_settings(use_empty=True)
	points, tangents = centre_line()
	build_banks(points, tangents)
	build_slime(points, tangents)
	bpy.context.preferences.filepaths.save_version = 0 # no .blend1 backup
	bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "slime_river.blend"))
	export = dict(filepath=os.path.join(out_dir, "slime_river.glb"), export_format="GLB",
			export_yup=True, export_apply=True, export_normals=True, export_texcoords=True)
	try:
		bpy.ops.export_scene.gltf(**export, export_vertex_color="ACTIVE", export_all_vertex_colors=False)
	except TypeError:
		bpy.ops.export_scene.gltf(**export, export_colors=True)
	print("saved", export["filepath"])


main()
