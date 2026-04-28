#ifndef ROAD_GENERATOR_H
#define ROAD_GENERATOR_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>

#include "../utils/curve/bezier_curve.h"
#include "../utils/curve/bspline_curve.h"
#include "../utils/curve/catmull_rom_curve.h"
#include "../utils/curve/natural_cubic_spline.h"

namespace godot {

class RoadGenerator : public Path3D {
	GDCLASS(RoadGenerator, Path3D)

public:
	enum CurveInterpolation {
		LINEAR,
		CUBIC,
		BSPLINE,
		CATMULLROM,
		NATURAL
	};

private:
	CurveInterpolation interpolation_mode = CATMULLROM;
	float scale = 0.5f;
	float distance_between_planks = 1.0f;
	bool show_debug_points = false;
	bool is_dirty = true;

	Path3D *debug_path = nullptr;
	MultiMeshInstance3D *plank_multimesh = nullptr;

	void _update_curve();
	void _update_planks();

protected:
	static void _bind_methods();

public:
	RoadGenerator();
	~RoadGenerator();

	void _ready() override;
	void _process(double delta) override;

	// Getters/Setters
	void set_interpolation_mode(CurveInterpolation p_mode);
	CurveInterpolation get_interpolation_mode() const;

	void set_scale(float p_scale);
	float get_scale() const;

	void set_distance_between_planks(float p_dist);
	float get_distance_between_planks() const;

	void set_show_debug_points(bool p_show);
	bool get_show_debug_points() const;
};

} // namespace godot

VARIANT_ENUM_CAST(RoadGenerator::CurveInterpolation);

#endif // ROAD_GENERATOR_H
