// noise(x) — smooth random blobs ("value noise"), computed with maths only (no textures).
//
//   1  i = floor(x), f = fract(x)        which grid cell are we in, and where inside it (0..1)
//   2  hash() the cell's 8 corners        one random value per corner (2D would be 4 corners)
//   3  f = f·f·(3 − 2f)                   smoothstep the position, so blending eases in and out
//                                         (plain linear blending shows creases along the grid)
//   4  mix() the corners by f             trilinear blend → soft blobs about one cell wide
// Bigger x scale = smaller blobs: noise(p * 0.25) has blobs ~4 m wide, noise(p * 2.0) ~0.5 m.
// The third coordinate is often a "layer" (a constant) or TIME (the pattern slowly morphs).
// Cost: 8 hash() calls ≈ 80 multiplies. For the cheap version see texture_noise.gdshaderinc.
#ifndef SHADER_LIB_VALUE_NOISE
#define SHADER_LIB_VALUE_NOISE
#include "res://shader_lib/hash.gdshaderinc"

float noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
			   mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

#endif
