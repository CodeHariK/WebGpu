// A spotlight's visible beam: an open cone drawn additively, fading toward its far end and at its
// silhouette, so it reads as light in haze. Cheap: no volumetric fog needed (mobile friendly).
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

uniform vec3 color : source_color = vec3(1.0, 0.3, 0.8);
uniform float strength = 0.6;
uniform float beam_length = 14.0; // cone height (m), the lamp end is at y = +beam_length / 2

varying float along; // 0 at the lamp … 1 at the far end

void vertex() {
	along = clamp(0.5 - VERTEX.y / beam_length, 0.0, 1.0);
}

void fragment() {
	float facing = abs(dot(normalize(NORMAL), normalize(VIEW)));
	float soft_edge = smoothstep(0.0, 0.6, facing);
	float fade = (1.0 - along) * (1.0 - along);
	float flicker = 0.9 + 0.1 * sin(TIME * 23.0 + along * 9.0);
	ALBEDO = color * strength * soft_edge * fade * flicker;
}
