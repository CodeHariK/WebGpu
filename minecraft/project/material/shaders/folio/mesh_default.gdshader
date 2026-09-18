shader_type spatial;

// =============================================================================
// Folio "MeshDefaultMaterial" port
//   folio-2025/sources/Game/Materials/MeshDefaultMaterial.js (TSL, WebGPU)
//
// Folio's material extends MeshLambertNodeMaterial but overrides outputNode to
// compute the final stylized colour itself. We mirror that with `unshaded`: we
// build the colour by hand in fragment() and write it straight to ALBEDO, so
// Godot's lighting never touches it. Every shared value comes from a global
// shader uniform published by a Folio* node (RenderingServer globals).
//
// Pipeline order is 1:1 with folio's outputNode:
//   base -> light bounce -> water -> light tint -> core+drop shadow -> fog
//        -> alpha-test discard -> reveal.
// =============================================================================

// Shaded (not unshaded) so the sun's light() pass can read real cast-shadow
// ATTENUATION; ambient disabled so DIFFUSE_LIGHT alone is the final colour.
render_mode cull_back, ambient_light_disabled;

// ---- Per-material inputs (folio _colorNode / _alphaNode / feature flags) -----
// _colorNode: the mesh's own colour (car paint, terrain, etc). White by default.
uniform vec4 base_color : source_color = vec4(1.0, 1.0, 1.0, 1.0);
uniform float alpha : hint_range(0.0, 1.0) = 1.0;
uniform float alpha_test = 0.1;

// folio per-material feature toggles (parameters.hasXxx). Default true.
uniform bool has_light_bounce = true;
uniform bool has_water = true;
uniform bool has_core_shadows = true;
uniform bool has_fog = true;
uniform bool has_reveal = true;
uniform bool has_drop_shadows = true; // real cast shadows, folded in light()

// ---- Shared global state (published by Folio* nodes) -------------------------
// FolioLighting
global uniform vec3 folio_light_direction;
global uniform vec4 folio_light_color;
global uniform float folio_light_intensity;
global uniform float folio_light_bounce_edge_low;
global uniform float folio_light_bounce_edge_high;
global uniform float folio_light_bounce_distance;
global uniform float folio_light_bounce_multiplier;
global uniform vec4 folio_shadow_color;
global uniform float folio_core_shadow_edge_low;
global uniform float folio_core_shadow_edge_high;

// FolioTerrain (ground-bounce tint)
global uniform sampler2D folio_terrain_gradient : filter_linear, repeat_disable;
global uniform sampler2D folio_terrain_data : filter_linear, repeat_disable;
global uniform vec4 folio_terrain_grass_color;
global uniform float folio_terrain_subdivision;

// FolioWater
global uniform float folio_water_surface_elevation;
global uniform float folio_water_surface_thickness;

// FolioFog
global uniform vec4 folio_fog_color_a;
global uniform vec4 folio_fog_color_b;
global uniform vec2 folio_fog_radial_center;
global uniform float folio_fog_radial_start;
global uniform float folio_fog_radial_end;
global uniform float folio_fog_near;
global uniform float folio_fog_far;

// FolioReveal
global uniform vec2 folio_reveal_center;
global uniform float folio_reveal_distance;
global uniform float folio_reveal_thickness;
global uniform vec4 folio_reveal_color;
global uniform float folio_reveal_intensity;

varying vec3 v_world_pos;
varying vec3 v_world_normal;

void vertex() {
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	// World normal (uniform-scale assumption; matches folio normalWorld).
	v_world_normal = normalize(mat3(MODEL_MATRIX) * NORMAL);
}

// folio terrain.terrainNode(worldXZ): sample the shared data map.
// Wheel-track carving (game.tracks) is deferred; default data is all-grass.
vec4 folio_terrain_sample(vec2 world_xz) {
	vec2 uv = world_xz / folio_terrain_subdivision / 1.5 + 0.5;
	return texture(folio_terrain_data, uv);
}

// folio terrain.colorNode(data): vertical gradient by elevation (b),
// blended toward flat grass by coverage (g).
vec3 folio_terrain_color(vec4 data) {
	vec3 base = texture(folio_terrain_gradient, vec2(0.0, 1.0 - data.b)).rgb;
	return mix(base, folio_terrain_grass_color.rgb, data.g);
}

void fragment() {
	vec3 base_col = base_color.rgb;
	vec3 out_col = base_color.rgb;
	// FrontSide only: no front-facing reorientation (folio flips for BackSide).
	vec3 n = normalize(v_world_normal);

	// --- Light bounce: ground colour reflected onto downward faces near ground
	if (has_light_bounce) {
		float bounce_orientation = smoothstep(
				folio_light_bounce_edge_low,
				folio_light_bounce_edge_high,
				dot(n, vec3(0.0, -1.0, 0.0)));
		float bounce_distance = pow(
				max(0.0,
						(folio_light_bounce_distance - max(0.0, v_world_pos.y))
						/ folio_light_bounce_distance),
				2.0);
		vec4 terrain_data = folio_terrain_sample(v_world_pos.xz);
		vec3 bounce_color = folio_terrain_color(terrain_data);
		out_col = mix(out_col, bounce_color,
				bounce_orientation * bounce_distance * folio_light_bounce_multiplier);
	}

	// --- Water: within surfaceThickness of the surface plane, flatten to white
	if (has_water) {
		// folio: nearWaterSurface = |y - elevation| > thickness; select keeps
		// the colour where 'true' (i.e. away from the surface), white where near.
		bool away_from_surface =
				abs(v_world_pos.y - folio_water_surface_elevation)
				> folio_water_surface_thickness;
		if (!away_from_surface) {
			out_col = vec3(1.0);
			base_col = vec3(1.0);
		}
	}

	// --- Light tint
	out_col *= folio_light_color.rgb * folio_light_intensity;

	// --- Core shadow: faces turned away from the sun
	float core_shadow_mix = 0.0;
	if (has_core_shadows) {
		core_shadow_mix = smoothstep(
				folio_core_shadow_edge_high,
				folio_core_shadow_edge_low,
				dot(n, folio_light_direction));
	}

	// --- Core shadow -> tint toward shadow colour. The DROP (cast) shadow is
	// added in light() where ATTENUATION is available; folio combines them as
	// max(core, drop), which light() reproduces exactly via a follow-up mix.
	if (has_core_shadows) {
		float combined = clamp(core_shadow_mix, 0.0, 1.0);
		vec3 shadow_col = base_col * folio_shadow_color.rgb;
		out_col = mix(out_col, shadow_col, combined);
	}

	// --- Fog: radial screen-space colour, range-based strength
	if (has_fog) {
		float color_mix = smoothstep(
				folio_fog_radial_start,
				folio_fog_radial_end,
				length(SCREEN_UV - folio_fog_radial_center));
		vec3 fog_col = mix(folio_fog_color_a.rgb, folio_fog_color_b.rgb, color_mix);
		float view_depth = -VERTEX.z; // view-space depth (folio rangeFogFactor)
		float strength = smoothstep(folio_fog_near, folio_fog_far, view_depth);
		out_col = mix(out_col, fog_col, strength);
	}

	// --- Alpha-test discard (folio alphaTest = 0.1)
	if (alpha < alpha_test) {
		discard;
	}

	// --- Reveal: expanding ring wipe from a world-space centre
	if (has_reveal) {
		float dist_to_center = length(v_world_pos.xz - folio_reveal_center);
		if (dist_to_center > folio_reveal_distance) {
			discard;
		}
		float reveal_mix = step(
				folio_reveal_distance - folio_reveal_thickness, dist_to_center);
		vec3 reveal_col = folio_reveal_color.rgb * folio_reveal_intensity;
		out_col = mix(out_col, reveal_col, reveal_mix);
	}

	// ALBEDO carries folio's colour (incl. core shadow, fog, reveal) into light(),
	// which reads it back to apply the cast shadow. With one sun + ambient disabled,
	// final pixel == DIFFUSE_LIGHT.
	ALBEDO = out_col;
}

// Recompute the base colour exactly as fragment() did (needed for the shadow tint).
vec3 folio_base_color() {
	vec3 base_col = base_color.rgb;
	if (has_water) {
		bool away_from_surface =
				abs(v_world_pos.y - folio_water_surface_elevation)
				> folio_water_surface_thickness;
		if (!away_from_surface) {
			base_col = vec3(1.0);
		}
	}
	return base_col;
}

void light() {
	// fragment() already applied the CORE shadow into ALBEDO. Here we add the
	// DROP (cast) shadow so the total tint equals folio's max(core, drop):
	// mixing toward shadow twice, mix(mix(x,s,core),s,t)=mix(x,s,core+t(1-core)),
	// so t = (drop-core)/(1-core) makes the effective mix exactly max(core,drop).
	vec3 shaded = ALBEDO;

	if (has_drop_shadows) {
		vec3 n = normalize(v_world_normal);
		float core = has_core_shadows
				? clamp(smoothstep(folio_core_shadow_edge_high,
						folio_core_shadow_edge_low, dot(n, folio_light_direction)),
						0.0, 1.0)
				: 0.0;
		// ATTENUATION for a directional light is its shadow term (1 lit, 0 shadowed).
		float drop = clamp(1.0 - ATTENUATION, 0.0, 1.0);
		float t = (drop > core) ? (drop - core) / max(1.0 - core, 1e-4) : 0.0;
		vec3 shadow_col = folio_base_color() * folio_shadow_color.rgb;
		shaded = mix(ALBEDO, shadow_col, t);
	}

	// Overwrite (single sun, ambient disabled): final colour == this.
	DIFFUSE_LIGHT = shaded;
}
