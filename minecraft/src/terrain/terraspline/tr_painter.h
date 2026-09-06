/**
 * @file tr_painter.h
 * @brief TerrainSplinePainter: writes a Terrain3D texture id along the parent spline's corridor.
 */
#ifndef TR_PAINTER_H
#define TR_PAINTER_H

#include "utils/spline3d/procedural_spline3d.h"
#include <cstdint>
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <vector>

namespace godot {

class TerrainSplineDeformer;

/**
 * @class TerrainSplinePainter
 * @brief SplineComponent that paints `texture_id` into the Terrain3D control map wherever the parent
 * spline's corridor weight is > 0: full strength on the core, fading over the falloff. The corridor is
 * either the sibling TerrainSplineDeformer's footprint (SHAPE_FROM_DEFORMER — road shoulders, river
 * banks and lake beds get their texture exactly where the earthwork is) or the painter's own
 * width / falloff (SHAPE_CUSTOM — a texture trail with no terrain change). `paint_curve` remaps the
 * weight (peak it in the middle for a band along the edge only). Runs inside the chunk job on any
 * thread (paint_prepared); the compositor uploads the result with the heightmap. Never touches heights.
 *
 * Control map bits (Terrain3D 1.0): base id 27..31, overlay id 22..26, blend 14..21 (0 = base,
 * 255 = overlay), autoshader bit 0. A paint of weight w over an existing pixel keeps the pixel's
 * dominant texture as the other layer, so painters stack: the later painter wins where it is strong.
 */
class TerrainSplinePainter : public SplineComponent {
	GDCLASS(TerrainSplinePainter,
			SplineComponent)

public:
	enum Shape { SHAPE_FROM_DEFORMER = 0, SHAPE_CUSTOM = 1 };

	/// Value of an unpainted control pixel: autoshader on, base texture 0 (Terrain3D's own default).
	static constexpr uint32_t CONTROL_DEFAULT = 1u;

private:
	int texture_id = 1; // Terrain3D texture slot, 0..31
	float strength = 1.0f; // Multiplies the corridor weight
	Shape shape = SHAPE_FROM_DEFORMER;
	float spline_width = 4.0f; // CUSTOM: half-width of the full-strength core
	float falloff_distance = 6.0f; // CUSTOM: ramp to zero outside the core
	bool fill_interior = true; // CUSTOM: closed loops paint their inside
	Ref<Curve> falloff_curve; // CUSTOM: remap of the ramp (x = 1 at the core, 0 at the edge)
	Ref<Curve> paint_curve; // Optional remap of the final weight before it is applied

	TerrainSplineDeformer *_shape = nullptr; // CUSTOM: private, tree-less deformer that owns the field code
	std::vector<float> _baked_paint_curve;
	bool _has_paint_curve = false;

	void _sync_shape();

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	TerrainSplinePainter();
	~TerrainSplinePainter();

	float get_spline_padding() const override { return shape == SHAPE_CUSTOM ? spline_width + falloff_distance : 0.0f; }

	void set_texture_id(int p_id) {
		texture_id = CLAMP(p_id, 0, 31);
		mark_dirty();
	}
	int get_texture_id() const { return texture_id; }
	void set_strength(float p_strength) {
		strength = CLAMP(p_strength, 0.0f, 1.0f);
		mark_dirty();
	}
	float get_strength() const { return strength; }
	void set_shape(Shape p_shape) {
		shape = p_shape;
		notify_property_list_changed();
		mark_dirty();
	}
	Shape get_shape() const { return shape; }
	void set_spline_width(float p_width) {
		spline_width = MAX(0.0f, p_width);
		mark_dirty();
	}
	float get_spline_width() const { return spline_width; }
	void set_falloff_distance(float p_dist) {
		falloff_distance = MAX(0.0f, p_dist);
		mark_dirty();
	}
	float get_falloff_distance() const { return falloff_distance; }
	void set_fill_interior(bool p_fill) {
		fill_interior = p_fill;
		mark_dirty();
	}
	bool get_fill_interior() const { return fill_interior; }
	void set_falloff_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_falloff_curve() const { return falloff_curve; }
	void set_paint_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_paint_curve() const { return paint_curve; }

	/// Marks the parent spline dirty so the compositor regenerates the affected chunks.
	void mark_dirty();
	void _on_curve_changed();

	/// Main thread, before the chunk job runs: syncs the private shape deformer and bakes the curves.
	void prepare();

	/**
	 * @brief Paints this component's texture into r_control (chunk_size² control words, row-major,
	 * z then x; allocated with CONTROL_DEFAULT here when empty). p_sibling is the parent spline's first
	 * TerrainSplineDeformer (may be null) — the footprint for SHAPE_FROM_DEFORMER. Any thread after
	 * prepare().
	 */
	void paint_prepared(
			std::vector<uint32_t> &r_control,
			ProceduralSpline3D *p_spline,
			const Vector2 &p_offset,
			int p_size,
			const Rect2 &p_padded_aabb,
			TerrainSplineDeformer *p_sibling
	);

	/// Control word after painting texture p_tex with weight p_weight (0..1] over p_old.
	static uint32_t paint_control(
			uint32_t p_old,
			int p_tex,
			float p_weight
	);
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TerrainSplinePainter::Shape);

#endif // TR_PAINTER_H
