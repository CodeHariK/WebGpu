#include "noises.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

static const char *SG_VORONOI = "folio_noise_voronoi";
static const char *SG_PERLIN = "folio_noise_perlin";
static const char *SG_HASH = "folio_noise_hash";

// --- CPU ports of the folio TSL noise functions (pure, periodic) --------------

static double fract1(double x) { return x - Math::floor(x); }
static double
pmod1(double x,
	  double m) {
	return x - m * Math::floor(x / m);
}

static Vector2 noise_hash(const Vector2 &p_in) {
	const Vector2 p(p_in.dot(Vector2(127.1, 311.7)), p_in.dot(Vector2(269.5, 183.3)));
	return Vector2(fract1(Math::sin(p.x) * 43758.5453123), fract1(Math::sin(p.y) * 43758.5453123));
}

static Vector2 noise_random(const Vector2 &p_in) {
	const Vector2 v(p_in.dot(Vector2(127.1, 311.7)), p_in.dot(Vector2(269.5, 183.3)));
	return Vector2(
			-1.0 + 2.0 * fract1(Math::sin(v.x) * 43758.5453123), -1.0 + 2.0 * fract1(Math::sin(v.y) * 43758.5453123)
	);
}

static Vector2 noise_modulo(
		const Vector2 &a,
		const Vector2 &m
) {
	const Vector2 pd(pmod1(a.x, m.x) + m.x, pmod1(a.y, m.y) + m.y);
	return Vector2(pmod1(pd.x, m.x), pmod1(pd.y, m.y));
}

// voronoi -> (minDist, edgeDist = minEdge - minDist, cellHash)
static Vector3 noise_voronoi(
		Vector2 uv,
		double repeat
) {
	uv *= repeat;
	const Vector2 i(Math::floor(uv.x), Math::floor(uv.y));
	const Vector2 f(fract1(uv.x), fract1(uv.y));
	double min_dist = 1.0;
	double min_edge = 1.0;
	Vector2 best_id(0, 0);
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			const Vector2 neighbor(x, y);
			const Vector2 cell(pmod1(i.x + neighbor.x, repeat), pmod1(i.y + neighbor.y, repeat));
			const Vector2 point = noise_hash(cell);
			const Vector2 diff = neighbor + (point - f);
			const double dist = diff.length();
			if (dist < min_dist) {
				min_edge = min_dist;
				min_dist = dist;
				best_id = i + neighbor;
			} else if (dist < min_edge) {
				min_edge = dist;
			}
		}
	}
	const Vector2 cell_id(fract1(best_id.x / repeat), fract1(best_id.y / repeat));
	return Vector3(min_dist, min_edge - min_dist, noise_hash(cell_id).x);
}

static double noise_perlin(
		Vector2 uv,
		double cell_amount,
		const Vector2 &period
) {
	uv *= cell_amount;
	Vector2 cmin(Math::floor(uv.x), Math::floor(uv.y));
	Vector2 cmax(Math::ceil(uv.x), Math::ceil(uv.y));
	cmin = noise_modulo(cmin, period);
	cmax = noise_modulo(cmax, period);
	const Vector2 blur(Math::smoothstep(0.0, 1.0, fract1(uv.x)), Math::smoothstep(0.0, 1.0, fract1(uv.y)));
	const Vector2 ll = noise_random(Vector2(cmin.x, cmin.y));
	const Vector2 lr = noise_random(Vector2(cmax.x, cmin.y));
	const Vector2 ul = noise_random(Vector2(cmin.x, cmax.y));
	const Vector2 ur = noise_random(Vector2(cmax.x, cmax.y));
	const Vector2 fr(fract1(uv.x), fract1(uv.y));
	const double bottom =
			Math::lerp((double)ll.dot(fr - Vector2(0, 0)), (double)lr.dot(fr - Vector2(1, 0)), (double)blur.x);
	const double top =
			Math::lerp((double)ul.dot(fr - Vector2(0, 1)), (double)ur.dot(fr - Vector2(1, 1)), (double)blur.x);
	return Math::lerp(bottom, top, (double)blur.y) * 0.8 + 0.5;
}

// -----------------------------------------------------------------------------

FolioNoises::FolioNoises() {}

FolioNoises::~FolioNoises() {}

void FolioNoises::_ready() {
	_generate();
	_register_globals();
}

void FolioNoises::_generate() {
	const int r = resolution;
	Ref<Image> vor = Image::create_empty(r, r, false, Image::FORMAT_RGBAF);
	Ref<Image> per = Image::create_empty(r, r, false, Image::FORMAT_RGBAF);
	Ref<Image> hsh = Image::create_empty(r, r, false, Image::FORMAT_RGBAF);

	for (int y = 0; y < r; y++) {
		for (int x = 0; x < r; x++) {
			const Vector2 uv((x + 0.5) / (double)r, (y + 0.5) / (double)r);

			const Vector3 v = noise_voronoi(uv, 8.0);
			vor->set_pixel(x, y, Color(v.x, v.y, v.z, 0.0));

			double p = noise_perlin(uv, 6.0, Vector2(6, 6));
			p = (p - 0.1) / 0.8; // remap(0.1, 0.9, 0, 1)
			per->set_pixel(x, y, Color(p, 0, 0, 0));

			const double h = noise_hash(uv).x;
			hsh->set_pixel(x, y, Color(h, 0, 0, 0));
		}
	}

	voronoi = ImageTexture::create_from_image(vor);
	perlin = ImageTexture::create_from_image(per);
	hash_texture = ImageTexture::create_from_image(hsh);
}

void FolioNoises::_register_globals() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	// Register once per process (add is runtime-safe; never use the editor-only get).
	static bool s_added = false;
	if (!s_added) {
		rs->global_shader_parameter_add(SG_VORONOI, RenderingServer::GLOBAL_VAR_TYPE_SAMPLER2D, Variant());
		rs->global_shader_parameter_add(SG_PERLIN, RenderingServer::GLOBAL_VAR_TYPE_SAMPLER2D, Variant());
		rs->global_shader_parameter_add(SG_HASH, RenderingServer::GLOBAL_VAR_TYPE_SAMPLER2D, Variant());
		s_added = true;
	}
	if (voronoi.is_valid()) {
		rs->global_shader_parameter_set(SG_VORONOI, voronoi);
	}
	if (perlin.is_valid()) {
		rs->global_shader_parameter_set(SG_PERLIN, perlin);
	}
	if (hash_texture.is_valid()) {
		rs->global_shader_parameter_set(SG_HASH, hash_texture);
	}
	globals_registered = true;
}

void FolioNoises::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_voronoi"), &FolioNoises::get_voronoi);
	ClassDB::bind_method(D_METHOD("get_perlin"), &FolioNoises::get_perlin);
	ClassDB::bind_method(D_METHOD("get_hash"), &FolioNoises::get_hash);
	ClassDB::bind_method(D_METHOD("get_resolution"), &FolioNoises::get_resolution);
}

} // namespace godot
