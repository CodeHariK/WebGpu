/**
 * @file stylized_terrain_mesh.h
 * @brief StylizedTerrainMesh: a self-generated, flat-shaded heightmap as a deterministic ArrayMesh.
 */
#ifndef STYLIZED_TERRAIN_MESH_H
#define STYLIZED_TERRAIN_MESH_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

/**
 * @class StylizedTerrainMesh
 * @brief Our own cartoon terrain, generated instead of Terrain3D so we own every step of the look.
 * A fractal-noise height field over a `size` metre patch (`cell_size` grid), pulled down at the edges
 * by `island_falloff`. `terrace` = true snaps each cell to a `step_height` band and builds crisp
 * vertical risers between neighbours (the Godus / hill-town look); `terrace` = false keeps the smooth
 * height and flat-shades each triangle (faceted low-poly hills). Either way the surface is flat-shaded
 * with per-band / per-height vertex colours (low_color -> high_color, risers darkened by `riser_darken`)
 * so the shared toon material lights it with no textures. Only parameters are saved; the mesh rebuilds
 * on change. Heights are shaped later by the spline deformers; for now it is noise + island mask.
 */
class StylizedTerrainMesh : public ArrayMesh {
	GDCLASS(StylizedTerrainMesh,
			ArrayMesh)

public:
	enum GridKind {
		GRID_SQUARE = 0, // Square cells split into 2 right triangles (uniform diagonal → slight grain)
		GRID_TRIANGULAR = 1 // Offset-row equilateral lattice (isotropic, no diagonal grain, even facets)
	};

private:
	int seed = 1337;
	Vector2 size = Vector2(128.0f, 128.0f); // Patch extent in metres (centred on the origin)
	float cell_size = 2.0f; // Grid spacing in metres
	float height_scale = 26.0f; // Peak height in metres
	float base_height = 0.0f; // Y offset of the whole patch

	// Height field
	float noise_frequency = 0.018f; // Cycles per metre (smaller = broader hills)
	int noise_octaves = 4;
	float noise_lacunarity = 2.0f;
	float noise_gain = 0.5f;
	bool ridged = false; // Sharp ridges / dunes instead of rounded hills
	float island_falloff = 0.6f; // 0 = flat edges, 1 = strong sink to the border (island)

	// Style
	bool terrace = true;
	float step_height = 3.0f; // Terrace band height in metres
	GridKind grid = GRID_SQUARE; // Lattice: square (grained) vs triangular (isotropic)

	// Surface detail: a low-amplitude noise displacement applied AFTER terracing, so the flat tops get
	// gentle height variation. The caps and the top/bottom of the extruded walls share the same XZ
	// displacement (they meet at the stitched contour points), so terrace edges stay welded — no cracks.
	// Keep `detail_amount` below `step_height` or it washes the terraces out.
	float detail_amount = 0.0f; // Metres of vertical variation on the surface (0 = dead-flat tops)
	float detail_frequency = 0.08f; // Cycles per metre of the detail noise

	// Colour
	Color low_color = Color(0.42f, 0.55f, 0.24f); // Valleys / lowest band
	Color high_color = Color(0.74f, 0.80f, 0.46f); // Peaks / highest band
	Color edge_color = Color(0.78f, 0.72f, 0.50f); // Blended in near sea level (beach / dry rim)
	float riser_darken = 0.28f; // How much darker a terrace riser is than its top
	float color_jitter = 0.05f; // Per-cell tint noise

	bool _rebuild_queued = false;
	float _min_h = 0.0f;
	float _max_h = 1.0f;

	// One contour segment produced while slicing a triangle: the iso-line at band boundary `k` (the top
	// of band k-1, the bottom of band k). Collected, stitched into loops, then extruded into walls.
	struct LevSeg {
		int k;
		Vector2 a;
		Vector2 b;
	};
	std::vector<PackedVector3Array> _contour_loops; // Stitched terrace outlines (3D, at their band height)

	float _height_raw(
			float p_wx,
			float p_wz
	) const; // Continuous height (noise + island), metres
	float _displace(
			float p_wx,
			float p_wz
	) const; // Post-terrace surface detail (metres); 0 when off
	Color _surface_color(
			float p_h_norm,
			float p_cx,
			float p_cz
	) const;
	// Builds the caps (flat plates) and border skirts, collects the contour segments, stitches them into
	// `_contour_loops`, and extrudes those loops into the terrace walls. Non-const: it fills the loops.
	void _build_terraced(
			int p_nx,
			int p_nz,
			PackedVector3Array &r_v,
			PackedVector3Array &r_n,
			PackedVector2Array &r_uv,
			PackedColorArray &r_c
	);
	/// Terrace one plan triangle: emit the flat caps + border skirts, and append each band-boundary
	/// contour crossing to `r_segs` (walls are extruded later from the stitched loops).
	void _emit_triangle(
			Vector2 p_a,
			float p_ha,
			Vector2 p_b,
			float p_hb,
			Vector2 p_c,
			float p_hc,
			float p_bottom,
			PackedVector3Array &r_v,
			PackedVector3Array &r_n,
			PackedVector2Array &r_uv,
			PackedColorArray &r_c,
			std::vector<LevSeg> &r_segs
	) const;
	/// Stitch same-level segments into loops, extrude them into walls, and store the loops (3D).
	void _build_walls(
			const std::vector<LevSeg> &p_segs,
			PackedVector3Array &r_v,
			PackedVector3Array &r_n,
			PackedVector2Array &r_uv,
			PackedColorArray &r_c
	);
	void _build_faceted(
			int p_nx,
			int p_nz,
			PackedVector3Array &r_v,
			PackedVector3Array &r_n,
			PackedVector2Array &r_uv,
			PackedColorArray &r_c
	) const;
	void _queue_rebuild();

protected:
	static void _bind_methods();
	/// Keep the baked geometry out of the .tscn: it rebuilds from the parameters at load.
	void _validate_property(PropertyInfo &p_property) const;

public:
	StylizedTerrainMesh();
	~StylizedTerrainMesh();

	void rebuild();
	/// Final surface height (terraced if `terrace`) at a world XZ, for placement / gameplay queries.
	float sample_height(const Vector2 &p_world) const;
	/// The terrace outlines from the last build: an Array of PackedVector3Array loops, each a closed (or
	/// border-open) contour at its band's height. Use them to run paths / walls / placement along a terrace.
	Array get_contours() const;

	// clang-format off
#define ST_PROP(m_type, m_name, m_clamp)      \
	void set_##m_name(m_type p_value) {       \
		m_type v = m_clamp;                   \
		if (m_name != v) {                    \
			m_name = v;                       \
			_queue_rebuild();                 \
		}                                     \
	}                                         \
	m_type get_##m_name() const { return m_name; }

	ST_PROP(int, seed, p_value)
	ST_PROP(Vector2, size, Vector2(CLAMP(p_value.x, 8.0f, 2048.0f), CLAMP(p_value.y, 8.0f, 2048.0f)))
	ST_PROP(float, cell_size, CLAMP(p_value, 0.5f, 16.0f))
	ST_PROP(float, height_scale, CLAMP(p_value, 0.0f, 512.0f))
	ST_PROP(float, base_height, p_value)
	ST_PROP(float, noise_frequency, CLAMP(p_value, 0.0005f, 0.5f))
	ST_PROP(int, noise_octaves, CLAMP(p_value, 1, 8))
	ST_PROP(float, noise_lacunarity, CLAMP(p_value, 1.2f, 4.0f))
	ST_PROP(float, noise_gain, CLAMP(p_value, 0.1f, 0.9f))
	ST_PROP(bool, ridged, p_value)
	ST_PROP(float, island_falloff, CLAMP(p_value, 0.0f, 1.0f))
	ST_PROP(bool, terrace, p_value)
	ST_PROP(float, step_height, CLAMP(p_value, 0.25f, 64.0f))
	ST_PROP(GridKind, grid, p_value)
	ST_PROP(float, detail_amount, MAX(0.0f, p_value))
	ST_PROP(float, detail_frequency, CLAMP(p_value, 0.005f, 1.0f))
	ST_PROP(Color, low_color, p_value)
	ST_PROP(Color, high_color, p_value)
	ST_PROP(Color, edge_color, p_value)
	ST_PROP(float, riser_darken, CLAMP(p_value, 0.0f, 1.0f))
	ST_PROP(float, color_jitter, CLAMP(p_value, 0.0f, 0.5f))
#undef ST_PROP
	// clang-format on
};

} // namespace godot

VARIANT_ENUM_CAST(godot::StylizedTerrainMesh::GridKind);

#endif // STYLIZED_TERRAIN_MESH_H
