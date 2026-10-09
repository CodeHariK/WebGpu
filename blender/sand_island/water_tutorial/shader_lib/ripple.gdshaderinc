// ripple(p, spacing) — wind ripples on sand: 0 in the troughs, 1 on the crests. p = world (x, z).
//
//   1  d = p.x·0.35 + p.y·0.94             distance along the wind direction (0.35, 0.94): equal
//                                           values form straight lines across the beach
//   2  fract(d / spacing)                   counts 0→1 once per ripple, then snaps back: a sawtooth,
//                                           i.e. straight parallel stripes `spacing` metres apart
//   3  + warp before fract                  noise shifts each stripe forward/back: big slow noise ×3
//                                           = sweeping curves, small noise ×0.6 = little wiggles
//   4  pow(phase, 3)                        stays low, rises steeply, drops: the real ripple shape
//                                           (long gentle windward slope, short steep lee face)
// Uses tex_noise2 (2 texture reads). To light the ripples, evaluate it twice more a few cm away and
// tilt the normal by the difference (sand step 9) — that triples its cost.
#ifndef SHADER_LIB_RIPPLE
#define SHADER_LIB_RIPPLE
#include "res://shader_lib/texture_noise.gdshaderinc"

float ripple(vec2 p, float spacing) {
	float warp = tex_noise2(p * 0.35) * 3.0 + tex_noise2(p * 1.1 + 4.0 * NOISE_LAYER) * 0.6;
	float phase = fract((p.x * 0.35 + p.y * 0.94) / spacing + warp);
	return pow(phase, 3.0);
}

#endif
