#include "road_generator.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void RoadGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_interpolation_mode", "mode"), &RoadGenerator::set_interpolation_mode);
	ClassDB::bind_method(D_METHOD("get_interpolation_mode"), &RoadGenerator::get_interpolation_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "interpolation_mode", PROPERTY_HINT_ENUM, "Linear,Cubic,BSpline,CatmullRom,Natural"), "set_interpolation_mode", "get_interpolation_mode");

	ClassDB::bind_method(D_METHOD("set_scale", "scale"), &RoadGenerator::set_scale);
	ClassDB::bind_method(D_METHOD("get_scale"), &RoadGenerator::get_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale"), "set_scale", "get_scale");

	ClassDB::bind_method(D_METHOD("set_distance_between_planks", "dist"), &RoadGenerator::set_distance_between_planks);
	ClassDB::bind_method(D_METHOD("get_distance_between_planks"), &RoadGenerator::get_distance_between_planks);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "distance_between_planks"), "set_distance_between_planks", "get_distance_between_planks");

	ClassDB::bind_method(D_METHOD("set_show_debug_points", "show"), &RoadGenerator::set_show_debug_points);
	ClassDB::bind_method(D_METHOD("get_show_debug_points"), &RoadGenerator::get_show_debug_points);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_debug_points"), "set_show_debug_points", "get_show_debug_points");

	BIND_ENUM_CONSTANT(LINEAR);
	BIND_ENUM_CONSTANT(CUBIC);
	BIND_ENUM_CONSTANT(BSPLINE);
	BIND_ENUM_CONSTANT(CATMULLROM);
	BIND_ENUM_CONSTANT(NATURAL);
}

RoadGenerator::RoadGenerator() {
}

RoadGenerator::~RoadGenerator() {
}

void RoadGenerator::_ready() {
	debug_path = Object::cast_to<Path3D>(get_node_or_null("DebugPath"));
	if (!debug_path) {
		debug_path = memnew(Path3D);
		debug_path->set_name("DebugPath");
		add_child(debug_path);
		debug_path->set_owner(get_owner());
		debug_path->set_debug_custom_color(Color(1, 0, 1)); // Bright magenta for visibility
	}

	plank_multimesh = Object::cast_to<MultiMeshInstance3D>(get_node_or_null("MultiMeshInstance3D"));
	if (!plank_multimesh) {
		plank_multimesh = memnew(MultiMeshInstance3D);
		plank_multimesh->set_name("MultiMeshInstance3D");
		add_child(plank_multimesh);
		plank_multimesh->set_owner(get_owner());
		
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		plank_multimesh->set_multimesh(mm);
	}

	is_dirty = true;
}

void RoadGenerator::_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint() || is_dirty) {
		if (is_dirty) {
			_update_curve();
			_update_planks();
			is_dirty = false;
		}
	}
}

void RoadGenerator::_update_curve() {
	if (!debug_path) return;

	Ref<Curve3D> source_curve = get_curve();
	if (source_curve.is_null()) return;

	int point_count = source_curve->get_point_count();
	if (point_count < 2) return;

	Ref<Curve3D> new_curve;
	new_curve.instantiate();

	std::vector<Vector3> points;
	for (int i = 0; i < point_count; ++i) {
		points.push_back(source_curve->get_point_position(i));
	}

	switch (interpolation_mode) {
		case LINEAR: {
			for (const auto &p : points) {
				new_curve->add_point(p);
			}
			break;
		}
		case CUBIC: {
			// In Godot, Cubic is the default behavior if we just copy handles
			for (int i = 0; i < point_count; ++i) {
				new_curve->add_point(points[i], source_curve->get_point_in(i), source_curve->get_point_out(i));
			}
			break;
		}
		case CATMULLROM: {
			// Generate many points to follow the Catmull-Rom spline
			int segments = point_count - 1;
			int samples_per_segment = 10;
			for (int i = 0; i < segments; ++i) {
				Vector3 p0 = points[MAX(0, i - 1)];
				Vector3 p1 = points[i];
				Vector3 p2 = points[i + 1];
				Vector3 p3 = points[MIN(point_count - 1, i + 2)];

				for (int j = 0; j < samples_per_segment; ++j) {
					float t = (float)j / (float)samples_per_segment;
					new_curve->add_point(CatmullRomCurve::evaluate(p0, p1, p2, p3, t, scale));
				}
			}
			new_curve->add_point(points.back());
			break;
		}
		case BSPLINE: {
			if (point_count < 4) break;
			int segments = point_count - 3;
			int samples_per_segment = 10;
			for (int i = 0; i < segments; ++i) {
				for (int j = 0; j < samples_per_segment; ++j) {
					float t = (float)j / (float)samples_per_segment;
					new_curve->add_point(BSplineCurve::evaluate_cubic(points[i], points[i+1], points[i+2], points[i+3], t));
				}
			}
			new_curve->add_point(BSplineCurve::evaluate_cubic(points[segments-1], points[segments], points[segments+1], points[segments+2], 1.0f));
			break;
		}
		case NATURAL: {
			NaturalCubicSpline spline;
			spline.set_points(points);
			int segments = point_count - 1;
			int samples_per_segment = 10;
			for (int i = 0; i < segments; ++i) {
				for (int j = 0; j < samples_per_segment; ++j) {
					float t = (float)i + (float)j / (float)samples_per_segment;
					new_curve->add_point(spline.evaluate(t));
				}
			}
			new_curve->add_point(spline.evaluate((float)segments));
			break;
		}
	}

	debug_path->set_curve(new_curve);
}

void RoadGenerator::_update_planks() {
	if (!debug_path || !plank_multimesh) return;

	Ref<Curve3D> curve = debug_path->get_curve();
	if (curve.is_null()) return;

	Ref<MultiMesh> mm = plank_multimesh->get_multimesh();
	if (mm.is_null()) return;

	float path_length = curve->get_baked_length();
	int count = (int)Math::floor(path_length / distance_between_planks);
	if (count <= 0) {
		mm->set_instance_count(0);
		return;
	}

	mm->set_instance_count(count);
	float offset = distance_between_planks * 0.5f;

	for (int i = 0; i < count; ++i) {
		float dist = offset + (float)i * distance_between_planks;
		Vector3 pos = curve->sample_baked(dist, true);
		Vector3 forward = (curve->sample_baked(dist + 0.1f, true) - pos).normalized();
		Vector3 up = curve->sample_baked_up_vector(dist, true);
		Vector3 right = forward.cross(up).normalized();
		up = right.cross(forward).normalized();

		Basis basis;
		basis.set_column(0, right);
		basis.set_column(1, up);
		basis.set_column(2, -forward);

		plank_multimesh->get_multimesh()->set_instance_transform(i, Transform3D(basis, pos));
	}
}

void RoadGenerator::set_interpolation_mode(CurveInterpolation p_mode) {
	interpolation_mode = p_mode;
	is_dirty = true;
}

RoadGenerator::CurveInterpolation RoadGenerator::get_interpolation_mode() const {
	return interpolation_mode;
}

void RoadGenerator::set_scale(float p_scale) {
	scale = p_scale;
	is_dirty = true;
}

float RoadGenerator::get_scale() const {
	return scale;
}

void RoadGenerator::set_distance_between_planks(float p_dist) {
	distance_between_planks = p_dist;
	is_dirty = true;
}

float RoadGenerator::get_distance_between_planks() const {
	return distance_between_planks;
}

void RoadGenerator::set_show_debug_points(bool p_show) {
	show_debug_points = p_show;
	is_dirty = true;
}

bool RoadGenerator::get_show_debug_points() const {
	return show_debug_points;
}

} // namespace godot
