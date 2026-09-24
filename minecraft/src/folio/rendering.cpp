#include "rendering.h"

#include <godot_cpp/classes/quad_mesh.hpp>

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

FolioRendering::FolioRendering() {}

FolioRendering::~FolioRendering() {}

void FolioRendering::_ready() {
	_build_environment();
	_build_dof();
	_build_background();
	apply_quality(level);
}

// Bloom via Godot's native additive glow (folio bloom: threshold 1, strength 0.25).
void FolioRendering::_build_environment() {
	env.instantiate();
	// Keep the existing background; we only add glow here (fog background is a
	// separate follow-up). A neutral clear keeps parity with the current look.
	env->set_background(Environment::BG_COLOR);
	env->set_bg_color(Color(0.34, 0.34, 0.34));

	env->set_glow_enabled(true);
	env->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_ADDITIVE);
	env->set_glow_intensity(1.0);
	env->set_glow_strength(bloom_strength);
	env->set_glow_bloom(0.0);
	env->set_glow_hdr_bleed_threshold(bloom_threshold);

	// Global look-grade lift: our folio materials are sun-only (ambient disabled), so
	// shadowed sides read darker than folio's web exposure. A modest exposure bump lifts
	// the whole image toward that brighter grade.
	env->set_tonemap_exposure((float)exposure);

	world_env = memnew(WorldEnvironment);
	world_env->set_name("FolioEnvironment");
	world_env->set_environment(env);
	add_child(world_env);
}

// cheapDOF fullscreen post layer (toggled by quality).
void FolioRendering::_build_dof() {
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/cheap_dof.gdshader");

	dof_material.instantiate();
	if (shader.is_valid()) {
		dof_material->set_shader(shader);
	}
	dof_material->set_shader_parameter("dof_start", dof_start);
	dof_material->set_shader_parameter("dof_end", dof_end);
	dof_material->set_shader_parameter("dof_repeats", dof_repeats);
	dof_material->set_shader_parameter("dof_amount", dof_amount);

	dof_rect = memnew(ColorRect);
	dof_rect->set_name("CheapDOF");
	dof_rect->set_anchors_preset(Control::PRESET_FULL_RECT);
	dof_rect->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	dof_rect->set_material(dof_material);

	dof_layer = memnew(CanvasLayer);
	dof_layer->set_name("FolioPost");
	dof_layer->set_layer(100); // above the game's 2D/UI
	dof_layer->add_child(dof_rect);
	add_child(dof_layer);
}

// Fog background: a fullscreen quad at the far plane drawing the radial fog colour
// (folio Fog `scene.backgroundNode`). Rendered behind all geometry.
void FolioRendering::_build_background() {
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/background.gdshader");

	background_material.instantiate();
	if (shader.is_valid()) {
		background_material->set_shader(shader);
	}
	// Draw before everything else so opaque geometry composites in front.
	background_material->set_render_priority(-100);

	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(1.0, 1.0));

	background = memnew(MeshInstance3D);
	background->set_name("FogBackground");
	background->set_mesh(quad);
	background->set_material_override(background_material);
	background->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	// The quad is pinned to clip space in the shader, so its world AABB is
	// meaningless — give it a huge custom AABB so it is never frustum-culled.
	background->set_custom_aabb(AABB(Vector3(-1e6, -1e6, -1e6), Vector3(2e6, 2e6, 2e6)));
	add_child(background);
}

void FolioRendering::apply_quality(int p_level) {
	level = p_level;

	// cheapDOF only at the high tier (folio level 0).
	if (dof_rect) {
		dof_rect->set_visible(level == 0);
	}

	// Bloom mip spread: folio uses nMips 5 (lvl0) vs 2 (lvl1). Godot has 7 glow
	// levels; enable a wider set at high quality, a narrow set at low.
	// Godot glow levels are 0-indexed (0..6). folio nMips: 5 @ high, 2 @ low.
	if (env.is_valid()) {
		const bool high = (level == 0);
		env->set_glow_level(0, 1.0);
		env->set_glow_level(1, 1.0);
		env->set_glow_level(2, high ? 1.0 : 0.0);
		env->set_glow_level(3, high ? 1.0 : 0.0);
		env->set_glow_level(4, high ? 1.0 : 0.0);
		env->set_glow_level(5, 0.0);
		env->set_glow_level(6, 0.0);
	}
}

void FolioRendering::_bind_methods() {
	ClassDB::bind_method(D_METHOD("apply_quality", "level"), &FolioRendering::apply_quality);
	ClassDB::bind_method(D_METHOD("get_environment"), &FolioRendering::get_environment);
}
