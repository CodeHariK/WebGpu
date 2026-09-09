/**
 * @file tr_water.cpp
 * @brief Shared toon water material (shader source lives here).
 */
#include "tr_water.h"
#include <godot_cpp/classes/shader.hpp>

namespace godot {

// Compiled once and shared by every river and lake. File scope (not a function-local static) so
// clear_water_material_cache() can release the RID before Godot's renderer shuts down.
namespace {
Ref<Shader> s_water_shader;
}

/**
 * @brief Built-in cartoon water: flat colour from the vertex colour, quantized stripes scrolled along UV.y by
 * `speed`, thin foam lines, bank foam from UV.x, a gentle vertex bob. Shared by rivers (TerrainSplineRoad
 * WATER profile) and lakes (TerrainSplineLake).
 */
Ref<ShaderMaterial> make_toon_water_material() {
	Ref<Shader> &shader = s_water_shader;
	if (shader.is_null()) {
		shader.instantiate();
		shader->set_code(R"(
shader_type spatial;
render_mode cull_back, depth_draw_opaque, specular_disabled;

uniform float speed = 0.6;
uniform float alpha : hint_range(0.0, 1.0) = 0.85;
uniform vec4 foam_color : source_color = vec4(0.95, 0.98, 1.0, 1.0);
uniform float stripe_repeat = 1.0;   // stripes per texture_length
uniform float bank_foam = 0.08;      // fraction of the width that foams at each bank
uniform float bob = 0.08;            // vertex bob amplitude, metres
global uniform int ts_debug_view;    // F3 debug views: 1 UV, 2 UV2, 3 raw vertex colour

varying vec2 v_uv;
varying vec3 v_color;

void vertex() {
	v_uv = UV;
	v_color = COLOR.rgb;
	VERTEX.y += sin(TIME * 1.7 + UV.y * 6.2831) * bob * float(NORMAL.y > 0.5);
}

void fragment() {
	// Distance along the river, scrolled by the flow speed (UV.y is in texture_length repeats).
	float t = v_uv.y * stripe_repeat - TIME * speed * 0.1;
	// Quantized stripes: two tones, then thin foam lines. Toon, not photoreal.
	float band = step(0.5, fract(t));
	vec3 base = mix(v_color, v_color * 1.12, band);
	float foam_line = smoothstep(0.92, 0.97, fract(t + 0.35 * sin(v_uv.x * 9.0)));
	// Bank foam: U runs around the closed profile; the deck is a contiguous span of it. Use the
	// lateral position encoded in UV.x relative to the deck's span (0.5..1.0 for the WATER profile).
	float lateral = clamp((v_uv.x - 0.5) * 2.0, 0.0, 1.0);
	float bank = smoothstep(bank_foam, 0.0, lateral) + smoothstep(1.0 - bank_foam, 1.0, lateral);
	float foam = max(foam_line, bank * (0.6 + 0.4 * sin(TIME * 2.0 + v_uv.y * 12.0)));
	ALBEDO = mix(base, foam_color.rgb, clamp(foam, 0.0, 1.0));
	ROUGHNESS = 0.2;
	ALPHA = alpha;
	if (ts_debug_view == 1) {
		ALBEDO = vec3(0.0); EMISSION = vec3(v_uv.x, fract(v_uv.y), 0.0); ALPHA = 1.0;
	} else if (ts_debug_view == 2) {
		ALBEDO = vec3(0.0); EMISSION = vec3(UV2.x, UV2.y, 0.0); ALPHA = 1.0;
	} else if (ts_debug_view == 3 || ts_debug_view == 4) {
		ALBEDO = vec3(0.0); EMISSION = ts_debug_view == 3 ? v_color : vec3(0.0); ALPHA = 1.0;
	}
}
)");
	}
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(shader);
	return m;
}

void clear_water_material_cache() { s_water_shader = Ref<Shader>(); }

} // namespace godot
