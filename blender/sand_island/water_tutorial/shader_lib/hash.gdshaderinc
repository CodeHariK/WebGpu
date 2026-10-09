// hash(p) — a random-looking number 0..1 for any position, the same every time for the same input.
// Feed it whole-number cell coordinates (floor(...)) to get ONE random number per grid cell.
//
//   1  p = fract(p * 0.3183099 + 0.1)   squash: ×1/π and keep the decimals → neat inputs (1, 2, 3)
//                                         become messy values in 0..1 (+0.1 avoids an exact 0)
//   2  p *= 17.0                         stretch to 0..17 so step 3 makes big numbers
//   3  fract(x·y·z·(x+y+z))              scramble: a product that swings wildly for tiny input changes;
//                                         keeping only its decimals looks random
// Cost: ~10 multiplies. Cheap alone; expensive when called many times (value_noise calls it 8×).
// See shader_lib/README.md and the function lab (function_lab.tscn, "hash").
#ifndef SHADER_LIB_HASH
#define SHADER_LIB_HASH

float hash(vec3 p) {
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

#endif
