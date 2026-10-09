// Texture versions of hash() and noise(): the randomness is pre-computed in a small texture of
// random grey pixels (textures/noise_128.png, one channel, 16 KB), so the GPU reads it instead of
// computing it. The includer must set the noise_tex uniform (import: Lossless, no mipmaps).
//
// tex_hash(cell)  one random number per grid cell = one exact pixel (texelFetch, no filtering).
//                  The 3D cell is folded onto the 2D texture: x and z pick the pixel, y shifts it.
// tex_noise2(x)   smooth 2D value noise in ONE read:
//   1  i = floor(x), f = fract(x)         grid cell and position inside it — like value_noise
//   2  f = f·f·(3 − 2f)                   smoothstep the position inside the cell
//   3  read at (i + f + 0.5) / size        the GPU's bilinear filter blends the 4 neighbouring
//                                          random pixels; because we moved the sample point by the
//                                          smoothstepped f, the blend eases like value_noise does
// tex_noise(x)    3D version for patterns that change over TIME: z picks a layer (an offset in the
//                  texture) and we blend two layers → 2 reads instead of 8 hashes.
// The pattern repeats every 128 cells; noise(p * 1.1) repeats every ~116 m.
#ifndef SHADER_LIB_TEXTURE_NOISE
#define SHADER_LIB_TEXTURE_NOISE

uniform sampler2D noise_tex : filter_linear, repeat_enable;
const float NOISE_SIZE = 128.0;
const vec2 NOISE_LAYER = vec2(37.0, 17.0);  // offset between independent "layers" of the texture

float tex_hash(vec3 cell) {
	ivec2 texel = ivec2(mod(cell.xz + cell.y * vec2(31.0, 17.0), NOISE_SIZE));
	return texelFetch(noise_tex, texel, 0).r;
}

float tex_noise2(vec2 x) {
	vec2 i = floor(x);
	vec2 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return textureLod(noise_tex, (i + f + 0.5) / NOISE_SIZE, 0.0).r;
}

float tex_noise(vec3 x) {
	float z = floor(x.z);
	float fz = fract(x.z);
	fz = fz * fz * (3.0 - 2.0 * fz);
	vec2 a = x.xy + z * NOISE_LAYER;
	return mix(tex_noise2(a), tex_noise2(a + NOISE_LAYER), fz);
}

#endif
