// MCGrid: particles that hide the water's seams, rebuilt with the water mesh (_rebuild_water).
//
//   WaterShoreFoam   small soft puffs along the line where the surface meets a bank (the seam
//                    between the water and the rock), drifting a little out onto the water
//   WaterSplash      spray thrown up where a fall lands (on the water below or on dry ground)
//
// Each is one GPUParticles3D (one draw call, simulated on the GPU) emitting from a points texture:
// the water tiles builder (mc_grid_water_tiles.cpp) samples the shore lines and the falls' feet
// every MCWaterArrays::PARTICLE_SPACING metres into MCWaterArrays::shore_* / splash_*, with a
// direction per point. Off with set_water_particles(false) (Stats → Diagnostics → Water particles).
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/particle_process_material.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <algorithm>

namespace godot {

namespace {
const int MAX_PARTICLES = 4000; // per emitter
const int TEXTURE_WIDTH = 1024; // points textures wrap onto more rows past this

// One emitter's look and motion.
struct ParticleStyle {
	const char *name;
	float size; // m, the quad
	float lifetime; // s
	int per_point; // particles alive per emission point
	float speed_min; // m/s along the point's direction
	float speed_max;
	float spread; // degrees around the direction
	float flatness; // 1: stays level (on the water)
	Vector3 gravity;
	float scale_min;
	float scale_max;
	float alpha; // peak opacity
};

const ParticleStyle SHORE = { "WaterShoreFoam", 0.22f, 1.8f, 2, 0.03f, 0.12f, 70.0f, 1.0f, Vector3(0, 0, 0), 0.6f, 1.3f, 0.55f };
const ParticleStyle SPLASH = { "WaterSplash", 0.3f, 0.9f, 5, 0.5f, 1.3f, 30.0f, 0.0f, Vector3(0, -3.0f, 0), 0.7f, 1.8f, 0.7f };

// A float texture of the points (RGB = xyz), TEXTURE_WIDTH wide.
Ref<ImageTexture> points_texture(const std::vector<Vector3> &p_points) {
	int count = static_cast<int>(p_points.size());
	int width = std::min(count, TEXTURE_WIDTH);
	int height = (count + width - 1) / width;
	PackedFloat32Array data;
	data.resize(static_cast<int64_t>(width) * height * 3);
	float *w = data.ptrw();
	for (int i = 0; i < width * height; i++) {
		const Vector3 &p = p_points[static_cast<size_t>(std::min(i, count - 1))];
		w[i * 3 + 0] = p.x;
		w[i * 3 + 1] = p.y;
		w[i * 3 + 2] = p.z;
	}
	Ref<Image> image = Image::create_from_data(width, height, false, Image::FORMAT_RGBF, data.to_byte_array());
	return ImageTexture::create_from_image(image);
}

// The quad's soft round puff: white, alpha from 1 in the middle to 0 at the rim.
Ref<StandardMaterial3D> puff_material() {
	Ref<Gradient> fade;
	fade.instantiate();
	fade->set_color(0, Color(1, 1, 1, 1));
	fade->set_color(1, Color(1, 1, 1, 0));
	Ref<GradientTexture2D> puff;
	puff.instantiate();
	puff->set_gradient(fade);
	puff->set_fill(GradientTexture2D::FILL_RADIAL);
	puff->set_fill_from(Vector2(0.5f, 0.5f));
	puff->set_fill_to(Vector2(0.5f, 0.0f));
	puff->set_width(64);
	puff->set_height(64);
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	material->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, puff);
	// soft particles: fade out where a puff meets the rock or the water, no hard cut line
	material->set_proximity_fade_enabled(true);
	material->set_proximity_fade_distance(0.12f);
	return material;
}

// Fade in quickly, hold, fade out.
Ref<GradientTexture1D> life_ramp(float p_alpha) {
	Ref<Gradient> ramp;
	ramp.instantiate();
	ramp->set_color(0, Color(1, 1, 1, 0));
	ramp->set_color(1, Color(1, 1, 1, 0));
	ramp->add_point(0.15f, Color(1, 1, 1, p_alpha));
	ramp->add_point(0.6f, Color(1, 1, 1, p_alpha * 0.7f));
	Ref<GradientTexture1D> texture;
	texture.instantiate();
	texture->set_gradient(ramp);
	return texture;
}
} // namespace

void MCGrid::set_water_particles(bool p_on) {
	water_particles = p_on;
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

// One emitter: made the first time, then its points, amount and bounds updated.
static GPUParticles3D *update_emitter(
		Node3D *p_parent,
		GPUParticles3D *p_node,
		const ParticleStyle &p_style,
		const std::vector<Vector3> &p_points,
		const std::vector<Vector3> &p_directions,
		bool p_on
) {
	if (!p_on || p_points.empty()) {
		if (p_node) {
			p_node->set_emitting(false);
			p_node->hide();
		}
		return p_node;
	}
	if (!p_node) {
		p_node = memnew(GPUParticles3D);
		p_node->set_name(p_style.name);
		p_node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		p_node->set_lifetime(p_style.lifetime);
		p_node->set_explosiveness_ratio(0.0f);
		p_node->set_randomness_ratio(0.5f);
		p_node->set_use_local_coordinates(true);
		Ref<QuadMesh> quad;
		quad.instantiate();
		quad->set_size(Vector2(p_style.size, p_style.size));
		quad->set_material(puff_material());
		p_node->set_draw_pass_mesh(0, quad);
		Ref<ParticleProcessMaterial> process;
		process.instantiate();
		process->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_DIRECTED_POINTS);
		process->set_spread(p_style.spread);
		process->set_flatness(p_style.flatness);
		process->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, p_style.speed_min);
		process->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, p_style.speed_max);
		process->set_param_min(ParticleProcessMaterial::PARAM_SCALE, p_style.scale_min);
		process->set_param_max(ParticleProcessMaterial::PARAM_SCALE, p_style.scale_max);
		process->set_gravity(p_style.gravity);
		process->set_color_ramp(life_ramp(p_style.alpha));
		p_node->set_process_material(process);
		p_parent->add_child(p_node);
	}
	Ref<ParticleProcessMaterial> process = p_node->get_process_material();
	process->set_emission_point_texture(points_texture(p_points));
	process->set_emission_normal_texture(points_texture(p_directions));
	process->set_emission_point_count(static_cast<int>(p_points.size()));
	int amount = std::clamp(static_cast<int>(p_points.size()) * p_style.per_point, 8, MAX_PARTICLES);
	if (p_node->get_amount() != amount) {
		p_node->set_amount(amount);
	}
	AABB box(p_points[0], Vector3());
	for (const Vector3 &p : p_points) {
		box.expand_to(p);
	}
	p_node->set_visibility_aabb(box.grow(1.5f));
	p_node->show();
	p_node->set_emitting(true);
	return p_node;
}

void MCGrid::_rebuild_water_particles(const MCWaterArrays &p_arrays) {
	water_shore_node =
			update_emitter(this, water_shore_node, SHORE, p_arrays.shore_points, p_arrays.shore_directions, water_particles);
	water_splash_node =
			update_emitter(this, water_splash_node, SPLASH, p_arrays.splash_points, p_arrays.splash_directions, water_particles);
}

} // namespace godot
