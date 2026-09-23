shader_type canvas_item;

// =============================================================================
// Speed lines + speed vignette — stylized sense-of-speed overlay.
//
// A single fullscreen ColorRect overlay (canvas_item) that does TWO effects in
// one pass, both faded in by `intensity` (0..1, driven by vehicle speed):
//
//   1. Speed lines  - thin radial streaks from the centre outward, masked so
//                     the middle stays clear and they live at the edges.
//   2. Speed vignette - a radial DARKENING of the periphery. This is the cheap
//                     stand-in for radial/zoom blur: instead of smearing pixels
//                     (many taps, bandwidth-heavy), we just darken the edges for
//                     a tunnel-vision "focus rushing forward" read, keeping the
//                     art crisp. One length + smoothstep, no extra pass.
//
// Cheap: one hash, no screen-texture read. Alpha-blends over the frame, so it
// composites without sampling the backbuffer. Folding both effects into this
// one pass means the vignette costs essentially nothing on top of the lines
// (fill-rate is the mobile bottleneck, and this is still a single overlay).
//
// The streaks composite OVER the vignette, so a bright streak is never darkened.
//
// Tuning:
//   density           - number of angular streak slots (more = finer lines)
//   inner_radius      - radius where streaks begin (keeps the middle clear)
//   outer_radius      - radius where streaks reach full strength
//   scroll_speed      - how fast the streaks pulse outward
//   line_color        - streak colour (white by default)
//   vignette_strength - max darkening at full intensity (0 disables it)
//   vignette_inner    - radius where the darkening starts
//   vignette_outer    - radius where the darkening is fully applied
//   vignette_color    - colour to darken toward (black by default)
// =============================================================================

uniform float intensity = 0.0;

// Speed lines.
uniform vec4 line_color : source_color = vec4(1.0, 1.0, 1.0, 1.0);
uniform float density = 90.0;
uniform float inner_radius = 0.30;
uniform float outer_radius = 0.85;
uniform float scroll_speed = 1.6;
uniform float thickness = 0.14; // 0..0.5 half-width of a streak within its slot

// Speed vignette (cheap radial-blur stand-in).
uniform float vignette_strength = 0.55; // 0 disables the darkening
uniform float vignette_inner = 0.40;
uniform float vignette_outer = 1.05;
uniform vec4 vignette_color : source_color = vec4(0.0, 0.0, 0.0, 1.0);

float hash(float n) {
	return fract(sin(n) * 43758.5453123);
}

void fragment() {
	if (intensity <= 0.001) {
		COLOR = vec4(0.0);
	} else {
		// Centre-relative UV, aspect-corrected so the effect stays radial on widescreen.
		vec2 uv = SCREEN_UV - 0.5;
		float aspect = SCREEN_PIXEL_SIZE.y / SCREEN_PIXEL_SIZE.x; // resX / resY
		uv.x *= aspect;

		float rad = length(uv) * 2.0; // ~0 centre .. ~1 edge
		float ang = atan(uv.y, uv.x); // -PI..PI
		float a = ang / TAU + 0.5; // 0..1 around the circle

		// --- Speed lines -----------------------------------------------------
		// Split the circle into streak slots; only some slots carry a streak.
		float slot = a * density;
		float id = floor(slot);
		float f = fract(slot) - 0.5; // -0.5..0.5 within the slot
		float rnd = hash(id);
		float present = step(0.45, hash(id + 7.0));

		// Thin line down the middle of the slot, thickness varied per streak.
		float half_w = mix(thickness * 0.4, thickness, rnd);
		float line = smoothstep(half_w, 0.0, abs(f)) * present;

		// Radial mask: clear centre, ramp to the edge.
		float edge = smoothstep(inner_radius, outer_radius, rad);

		// Pulse brightness outward so the streaks feel like they are moving.
		float scroll = fract(rad * 1.5 - TIME * scroll_speed * (0.6 + rnd));
		float pulse = 0.5 + 0.5 * smoothstep(0.25, 1.0, scroll);

		float line_a = clamp(intensity * edge * line * pulse, 0.0, 1.0) * line_color.a;

		// --- Speed vignette --------------------------------------------------
		float vign = smoothstep(vignette_inner, vignette_outer, rad);
		float vign_a = clamp(vign * intensity * vignette_strength, 0.0, 1.0) * vignette_color.a;

		// --- Composite lines OVER vignette (straight-alpha "over") -----------
		float out_a = line_a + vign_a * (1.0 - line_a);
		vec3 out_rgb = (line_color.rgb * line_a + vignette_color.rgb * vign_a * (1.0 - line_a)) / max(out_a, 0.0001);

		COLOR = vec4(out_rgb, out_a);
	}
}
