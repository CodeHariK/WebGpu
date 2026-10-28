// The cultist burning away when it dies (src/enemy/dissolve.gd drives `progress` 0 → 1).
//
// Each pixel looks up a cloudy noise value (0..1). Pixels whose value is below the cut line are
// thrown away (discard), so holes open where the noise is darkest and spread as the cut rises.
// Pixels just above the cut glow blue, the colour of its marks: the burning edge.
//
//   progress    0 = whole body, 1 = gone (the cut runs a little past both ends so it starts
//               and finishes cleanly)
//   edge_width  how far above the cut the glow reaches (in noise units)
//
// Only used while a body dissolves; the living cultist uses materials/cultist_body.tres.
shader_type spatial;

uniform sampler2D albedo_tex : source_color, filter_linear_mipmap, repeat_enable;
uniform sampler2D normal_tex : hint_normal, filter_linear_mipmap, repeat_enable;
uniform sampler2D noise_tex : filter_linear, repeat_enable;
uniform float progress : hint_range(0.0, 1.0) = 0.0;
uniform float edge_width = 0.06;
uniform vec3 edge_color : source_color = vec3(0.3, 0.55, 1.0);

void fragment() {
	float noise = texture(noise_tex, UV * 3.0).r;
	float cut = mix(-0.05, 1.05, progress);
	if (noise < cut) {
		discard;
	}
	ALBEDO = texture(albedo_tex, UV).rgb;
	NORMAL_MAP = texture(normal_tex, UV).rgb;
	ROUGHNESS = 0.7;
	float edge = 1.0 - smoothstep(0.0, edge_width, noise - cut);
	EMISSION = edge_color * edge * 6.0;
}
