/**
 * @file prop_geometry.h
 * @brief Header-only helpers shared by the procedural props: seeded RNG, icosphere, value noise.
 *
 * Everything here is `inline` / internal-linkage so each prop .cpp gets its own copy; there is no
 * prop_geometry.cpp. Keep it small — anything that grows a lifecycle belongs in its own file.
 */
#ifndef PROP_GEOMETRY_H
#define PROP_GEOMETRY_H

#include <cstdint>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {
namespace prop {

/// PCG-style generator (high bits, unlike an LCG's cycling low bits). Deterministic per seed.
struct RNG {
	uint64_t state;
	explicit RNG(uint64_t p_seed) : state(p_seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
	uint32_t next() {
		uint64_t old = state;
		state = old * 6364136223846793005ULL + 1442695040888963407ULL;
		uint32_t xs = (uint32_t)(((old >> 18u) ^ old) >> 27u);
		uint32_t rot = (uint32_t)(old >> 59u);
		return (xs >> rot) | (xs << ((-rot) & 31));
	}
	float unit() { return (next() >> 8) * (1.0f / 16777216.0f); }
	float
	range(float a,
		  float b) {
		return a + (b - a) * unit();
	}
	/// Uniform direction on the unit sphere.
	Vector3 dir() {
		float z = range(-1.0f, 1.0f);
		float a = range(0.0f, (float)Math::TAU);
		float r = Math::sqrt(MAX(0.0f, 1.0f - z * z));
		return Vector3(r * Math::cos(a), z, r * Math::sin(a));
	}
};

/// Unit icosphere: positions on the sphere and outward-wound triangles, subdivided p_detail times
/// (0: 12 verts / 20 tris, 1: 42 / 80, 2: 162 / 320, 3: 642 / 1280).
struct IcoSphere {
	std::vector<Vector3> verts;
	std::vector<int> tris;
	explicit IcoSphere(int p_detail) {
		const float t = (1.0f + Math::sqrt(5.0f)) * 0.5f;
		auto add = [&](float x, float y, float z) { verts.push_back(Vector3(x, y, z).normalized()); };
		add(-1, t, 0);
		add(1, t, 0);
		add(-1, -t, 0);
		add(1, -t, 0);
		add(0, -1, t);
		add(0, 1, t);
		add(0, -1, -t);
		add(0, 1, -t);
		add(t, 0, -1);
		add(t, 0, 1);
		add(-t, 0, -1);
		add(-t, 0, 1);
		const int f[] = { 0, 11, 5,	 0, 5,	1, 0, 1, 7, 0, 7,  10, 0, 10, 11, 1, 5, 9, 5, 11,
						  4, 11, 10, 2, 10, 7, 6, 7, 1, 8, 3,  9,  4, 3,  4,  2, 3, 2, 6, 3,
						  6, 8,	 3,	 8, 9,	4, 9, 5, 2, 4, 11, 6,  2, 10, 8,  6, 7, 9, 8, 1 };
		tris.assign(f, f + 60);
		for (int d = 0; d < p_detail; ++d) {
			std::vector<int> out;
			HashMap<int64_t, int> mid;
			auto midp = [&](int a, int b) -> int {
				int64_t key = a < b ? ((int64_t)a << 32) | b : ((int64_t)b << 32) | a;
				if (mid.has(key)) {
					return mid[key];
				}
				int idx = verts.size();
				verts.push_back(((verts[a] + verts[b]) * 0.5f).normalized());
				mid[key] = idx;
				return idx;
			};
			for (size_t i = 0; i < tris.size(); i += 3) {
				int a = tris[i], b = tris[i + 1], c = tris[i + 2];
				int ab = midp(a, b), bc = midp(b, c), ca = midp(c, a);
				const int q[] = { a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca };
				out.insert(out.end(), q, q + 12);
			}
			tris.swap(out);
		}
	}
};

/// Integer lattice hash → [0, 1).
inline float
hash3(int x,
	  int y,
	  int z,
	  uint32_t seed) {
	uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)z * 2147483647u + seed * 3266489917u;
	h = (h ^ (h >> 13)) * 1274126177u;
	h ^= h >> 16;
	return (h & 0xFFFFFF) * (1.0f / 16777216.0f);
}

/// Smooth value noise in [-1, 1].
inline float value_noise(
		const Vector3 &p,
		uint32_t seed
) {
	const int ix = (int)Math::floor(p.x), iy = (int)Math::floor(p.y), iz = (int)Math::floor(p.z);
	float fx = p.x - ix, fy = p.y - iy, fz = p.z - iz;
	fx = fx * fx * (3.0f - 2.0f * fx);
	fy = fy * fy * (3.0f - 2.0f * fy);
	fz = fz * fz * (3.0f - 2.0f * fz);
	auto h = [&](int dx, int dy, int dz) { return hash3(ix + dx, iy + dy, iz + dz, seed); };
	const float x00 = Math::lerp(h(0, 0, 0), h(1, 0, 0), fx), x10 = Math::lerp(h(0, 1, 0), h(1, 1, 0), fx);
	const float x01 = Math::lerp(h(0, 0, 1), h(1, 0, 1), fx), x11 = Math::lerp(h(0, 1, 1), h(1, 1, 1), fx);
	const float y0 = Math::lerp(x00, x10, fy), y1 = Math::lerp(x01, x11, fy);
	return Math::lerp(y0, y1, fz) * 2.0f - 1.0f;
}

/// Fractal sum of value noise, normalized to about [-1, 1]. `ridged` turns troughs into sharp creases.
inline float
fbm(Vector3 p,
	int octaves,
	float lacunarity,
	float gain,
	uint32_t seed,
	bool ridged) {
	float sum = 0.0f, amp = 1.0f, norm = 0.0f;
	for (int i = 0; i < octaves; ++i) {
		float n = value_noise(p, seed + (uint32_t)i * 7919u);
		if (ridged) {
			n = 1.0f - Math::abs(n) * 2.0f; // Creases at the zero crossings
		}
		sum += n * amp;
		norm += amp;
		amp *= gain;
		p *= lacunarity;
	}
	return norm > 0.0f ? sum / norm : 0.0f;
}

} // namespace prop
} // namespace godot

#endif // PROP_GEOMETRY_H
