// STEP 1 — plain: one colour, toon light. This is the "flat rocks" problem.
// Every facet has the same colour, and with a two-band toon light every facet that faces the sun
// becomes the SAME lit colour and every other facet the SAME shadow colour. The shape is there in the
// mesh, but neighbouring faces that the light hits about equally merge into one flat blob. Every
// later step adds one painterly trick to separate the facets, with no texture made for each rock.
shader_type spatial;

uniform vec3 rock_colour : source_color = vec3(0.74, 0.68, 0.62);
uniform vec3 shadow_tint : source_color = vec3(0.6, 0.58, 0.8);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.35;

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	ALBEDO = rock_colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

// light(): how much light, and what colour, reaches this pixel. Godot calls it once per pixel for
// EVERY light that touches it (here only the sun) and afterwards shows about ALBEDO × DIFFUSE_LIGHT
// (+ the environment's ambient light).
//
// 1  dot(NORMAL, LIGHT)  how directly the surface faces the sun. NORMAL = where the surface points,
//    LIGHT = direction towards the sun, both length 1, so the dot is the cosine of the angle:
//      facing the sun 1.0 · at 60° 0.5 · edge-on (90°) 0.0 · facing away negative
//    That alone is ordinary "Lambert" shading: brightness fades smoothly as a face turns away.
// 2  × ATTENUATION       is something blocking the sun? 1 = in sunlight, 0 = inside a cast shadow
//    (under an overhang, behind another rock), in between on soft shadow edges. A sun-facing pixel
//    inside a shadow becomes 0, so cast shadows and faces turned away are treated the same.
// 3  smoothstep(0.15, 0.3, …)  THE TOON STEP. Instead of a smooth gradient, everything is pushed
//    to 0 or 1: below 0.15 → lit = 0 (shadow), above 0.3 → lit = 1 (lit), a short soft blend between
//    so the shadow line is not jagged.
//      0.9 → 1 · 0.5 → 1 · 0.22 → ≈0.5 (the edge) · 0.1 → 0 · −0.4 → 0
//    Narrow it (0.2, 0.22) for a razor-sharp edge; widen it (0.0, 0.6) and it is almost realistic.
// 4  mix(shadow_tint * shadow_light, vec3(1.0), lit)  the colour of each band:
//      lit = 1 → vec3(1.0): white, full light
//      lit = 0 → (0.6, 0.58, 0.8) × 0.35 ≈ (0.21, 0.20, 0.28): a dim BLUISH light, not black.
//    Real shadows are lit by the blue sky, so painters make them cool instead of grey.
//    shadow_light sets how dark the shadow side is.
// 5  LIGHT_COLOR / PI    LIGHT_COLOR = sun colour × energy, and in a custom light() Godot has already
//    multiplied it by π. Dividing by π cancels that, so a white sun at energy 1 gives exactly 1.0.
//    Without it everything is ~3× too bright and clips to white.
// 6  DIFFUSE_LIGHT +=    += because light() runs once per light and the results add up (two lamps
//    light a pixel twice).
//
// Worked example, ALBEDO = rock_colour ≈ (0.74, 0.68, 0.62):
//   top, facing the sun   lit 1 → light (1, 1, 1)          → shown ≈ (0.74, 0.68, 0.62) warm stone
//   side, turned away     lit 0 → light (0.21, 0.20, 0.28) → shown ≈ (0.16, 0.14, 0.17) dark, cool
//   (the shadow side ends up a bit lighter on screen: the ambient light adds on top)
// Every face is either "lit" or "the same blue shadow": that two-tone look IS the flat-rock problem
// this step shows. Each later step adds one idea to break it up.
void light() {
	float lit = smoothstep(0.15, 0.3, dot(NORMAL, LIGHT) * ATTENUATION);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(shadow_tint * shadow_light, vec3(1.0), lit);
}
