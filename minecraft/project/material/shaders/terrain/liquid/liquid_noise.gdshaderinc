// Noise for the liquid shaders (water, lava, poison). Pure maths, no textures. Their working
// copies, and a desktop look of each, are in the Liquid Shaders project
// (blender/sand_island/liquid/).
//
// hash / hash2: a random number (two for hash2) for a whole point of the plane. No sin(): sin of a
// big number loses precision on some GPUs (Metal), and with coordinates of tens of metres that
// turns patterns into speckle (Dave Hoskins' hash without sine).
// noise: value noise: a random number at every whole point, blended smoothly in between, so it
// looks like soft blobs, 0..1.
#ifndef LIQUID_NOISE
#define LIQUID_NOISE

float hash(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

vec2 hash2(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.xx + p3.yz) * p3.zy);
}

float noise(vec2 p) {
	vec2 i = floor(p); // the whole point at the cell's corner
	vec2 f = fract(p); // where in the cell, 0..1
	vec2 u = f * f * (3.0 - 2.0 * f); // eased, so the blend has no creases
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

#endif
