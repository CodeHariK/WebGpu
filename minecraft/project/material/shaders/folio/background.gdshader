shader_type spatial;

// =============================================================================
// Folio fog background  (folio Fog.js: `scene.backgroundNode = this.color`)
//
// folio fills the scene background with the SAME radial screen-space gradient the
// fog uses, so empty/distant pixels fade into the (day-cycle-animated) fog colour
// instead of a flat clear. We draw it as a fullscreen quad pinned to the far plane
// (POSITION overridden in clip space), rendered first and depth-neutral so all 3D
// geometry composites in front of it.
//
//   colorMix = smoothstep(radialStart, radialEnd, |SCREEN_UV - radialCenter|)
//   background = mix(fogColorA, fogColorB, colorMix)
// =============================================================================

// Depth test ENABLED + writes far depth; render_priority (-100, set on the material)
// draws it first, so opaque geometry (nearer) composites in front of it.
render_mode unshaded, cull_disabled, depth_draw_opaque, shadows_disabled, fog_disabled;

global uniform vec4 folio_fog_color_a;
global uniform vec4 folio_fog_color_b;
global uniform vec2 folio_fog_radial_center;
global uniform float folio_fog_radial_start;
global uniform float folio_fog_radial_end;

void vertex() {
	// QuadMesh verts are ±0.5; ×2 → ±1 NDC. z = w = 1 → the far plane.
	POSITION = vec4(VERTEX.x * 2.0, VERTEX.y * 2.0, 0.0, 1.0); // reverse-Z far plane
}

void fragment() {
	float color_mix = smoothstep(folio_fog_radial_start, folio_fog_radial_end,
			length(SCREEN_UV - folio_fog_radial_center));
	ALBEDO = mix(folio_fog_color_a.rgb, folio_fog_color_b.rgb, color_mix);
}
