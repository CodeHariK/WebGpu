shader_type canvas_item;

// =============================================================================
// Slow-mo grade + low-time warning — GTA-style bullet-time screen treatment.
//
// A fullscreen ColorRect overlay (canvas_item) that reads the rendered frame and
// does two things in one pass, driven by two uniforms:
//
//   amount (0..1) - slow-mo grade: desaturate the frame and push a cool tint,
//                   the classic "time dilation" look. Mixed over the original by
//                   `amount`, so 0 = untouched.
//   warn   (0..1) - low-time warning: a red edge flash that pulses over time,
//                   ramping up as the bullet-time meter runs out.
//
// Reads the screen via hint_screen_texture (Godot auto-copies the backbuffer for
// a canvas shader, same as cheap_dof). The owning node hides this overlay when
// both amount and warn are ~0 so the backbuffer copy is skipped when idle.
//
// Tuning:
//   desaturate   - how grey the grade goes (0 keeps colour, 1 full greyscale)
//   tint         - colour multiplied in while slowed (cool blue by default)
//   warn_color   - edge flash colour (red by default)
//   warn_inner   - radius where the warning flash begins
//   warn_pulse   - flash pulse speed (rad/s)
// =============================================================================

uniform sampler2D screen_tex : hint_screen_texture, filter_linear;

uniform float amount = 0.0; // slow-mo grade strength
uniform float warn = 0.0; // low-time warning strength

uniform float desaturate = 0.65;
uniform vec4 tint : source_color = vec4(0.62, 0.75, 1.0, 1.0);

uniform vec4 warn_color : source_color = vec4(1.0, 0.12, 0.08, 1.0);
uniform float warn_inner = 0.35;
uniform float warn_outer = 1.15;
uniform float warn_pulse = 9.0;
uniform float warn_max = 0.6; // cap on the red mix at full warning

void fragment() {
	vec3 src = texture(screen_tex, SCREEN_UV).rgb;

	// --- Slow-mo grade: desaturate toward luma, then cool tint ---------------
	float luma = dot(src, vec3(0.299, 0.587, 0.114));
	vec3 graded = mix(src, vec3(luma), desaturate) * tint.rgb;
	vec3 col = mix(src, graded, clamp(amount, 0.0, 1.0));

	// --- Low-time warning: pulsing red edge flash ----------------------------
	if (warn > 0.001) {
		vec2 uv = SCREEN_UV - 0.5;
		float aspect = SCREEN_PIXEL_SIZE.y / SCREEN_PIXEL_SIZE.x;
		uv.x *= aspect;
		float rad = length(uv) * 2.0;
		float edge = smoothstep(warn_inner, warn_outer, rad);
		float pulse = 0.5 + 0.5 * sin(TIME * warn_pulse);
		float w = clamp(warn, 0.0, 1.0) * edge * pulse * warn_max;
		col = mix(col, warn_color.rgb, w);
	}

	COLOR = vec4(col, 1.0);
}
