/**
 * @file tr_bench.cpp
 * @brief Benchmark mode for TerrainSplineCompositor (Terraspline.md, "Benchmark protocol").
 *
 * Enabled with the user arg `--terraspline-bench`: generates a fixed 5x5 block of chunks around the
 * origin, records how the main-thread work is distributed over frames, prints a per-frame timeline
 * and one `[BENCH]` summary line (including a content hash of all heightmaps and scatter transforms,
 * so refactors can prove "same output"), then quits. Extra flags:
 *   --terraspline-no-field     force the legacy deformer path (A/B)
 *   --terraspline-dump=<dir>   write each chunk's raw float heightmap as chunk_x_z.bin
 * Every entry point is gated on `_bench`, so the mode costs nothing when off.
 */
#include "tr_compositor.h"
#include "tr_deformer.h"
#include <algorithm>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static constexpr int BENCH_HALF = 2; // 5x5 chunks

void TerrainSplineCompositor::_bench_init() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
	bool no_field = false;
	for (int i = 0; i < args.size(); i++) {
		if (args[i] == "--terraspline-bench") {
			_bench = true;
		} else if (args[i] == "--terraspline-no-field") {
			no_field = true;
		} else if (args[i].begins_with("--terraspline-dump=")) {
			_bench_dump_dir = args[i].substr(String("--terraspline-dump=").length());
		}
	}
	if (!_bench) {
		return;
	}

	if (no_field) {
		for (ProceduralSpline3D *spline : _gather_splines()) {
			TypedArray<Node> comps = spline->get_children();
			for (int j = 0; j < comps.size(); j++) {
				TerrainSplineDeformer *d = Object::cast_to<TerrainSplineDeformer>(comps[j]);
				if (d) {
					d->set_use_distance_field(false);
				}
			}
		}
		UtilityFunctions::print("[BENCH] distance field DISABLED (legacy evaluation)");
	}

	for (int cx = -BENCH_HALF; cx <= BENCH_HALF; cx++) {
		for (int cz = -BENCH_HALF; cz <= BENCH_HALF; cz++) {
			_bench_chunks.push_back(Vector2i(cx, cz));
		}
	}
	_bench_start_usec = Time::get_singleton()->get_ticks_usec();
	_bench_first_frame = Engine::get_singleton()->get_process_frames();
	UtilityFunctions::print("[BENCH] started: ", (int)_bench_chunks.size(), " chunks of ", chunk_size, " m");
}

void TerrainSplineCompositor::_bench_dump_chunk(const Ref<TerrainChunk> &p_chunk) {
	if (_bench_dump_dir.is_empty() || p_chunk.is_null() || p_chunk->get_heightmap().is_null()) {
		return;
	}
	Vector2i c = p_chunk->get_chunk_coords();
	String path = _bench_dump_dir.path_join(vformat("chunk_%d_%d.bin", c.x, c.y));
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	if (f.is_null()) {
		return;
	}
	Ref<Image> img = p_chunk->get_heightmap()->get_image();
	f->store_buffer(img->get_data());
}

bool TerrainSplineCompositor::_bench_wants_chunk(const Vector2i &p_chunk) const {
	return std::find(_bench_chunks.begin(), _bench_chunks.end(), p_chunk) != _bench_chunks.end();
}

void TerrainSplineCompositor::_bench_add_frame_time(uint64_t p_usec) {
	if (!_bench || _bench_done) {
		return;
	}
	uint64_t frame = Engine::get_singleton()->get_process_frames();
	_bench_frame_ms[frame] = (_bench_frame_ms.has(frame) ? _bench_frame_ms[frame] : 0.0) + p_usec / 1000.0;
}

/**
 * @brief FNV-1a over every bench chunk's heightmap (in coordinate order), its cached physics
 * transforms and its MultiMesh buffers. Identical hash across builds == identical terrain and
 * scatter output. Also returns the total scatter instance count.
 */
uint64_t TerrainSplineCompositor::_bench_content_hash(int &r_total_instances) const {
	uint64_t hash = 1469598103934665603ULL;
	auto fnv = [&](const void *p, size_t n) {
		const uint8_t *b = (const uint8_t *)p;
		for (size_t i = 0; i < n; i++) {
			hash ^= b[i];
			hash *= 1099511628211ULL;
		}
	};

	std::vector<Vector2i> coords(_bench_chunks);
	std::sort(coords.begin(), coords.end(), [](const Vector2i &a, const Vector2i &b) {
		return a.x != b.x ? a.x < b.x : a.y < b.y;
	});

	r_total_instances = 0;
	for (const Vector2i &c : coords) {
		Ref<TerrainChunk> chunk = chunk_buffers[c];
		PackedByteArray bytes = chunk->get_heightmap()->get_image()->get_data();
		fnv(bytes.ptr(), bytes.size());
		for (const TerrainChunk::PhysicsCache &pc : chunk->get_physics_caches()) {
			for (const Transform3D &t : pc.transforms) {
				fnv(&t, sizeof(Transform3D));
				r_total_instances++;
			}
		}
		for (uint64_t id : chunk->get_visual_nodes()) {
			MultiMeshInstance3D *mmi = Object::cast_to<MultiMeshInstance3D>(ObjectDB::get_instance(id));
			if (mmi && mmi->get_multimesh().is_valid()) {
				PackedFloat32Array buf = mmi->get_multimesh()->get_buffer();
				fnv(buf.ptr(), buf.size() * sizeof(float));
				r_total_instances += mmi->get_multimesh()->get_instance_count();
			}
		}
	}
	return hash;
}

/// Once every bench chunk is resident and uploaded, prints the summary and quits.
void TerrainSplineCompositor::_bench_check_done() {
	if (!_bench || _bench_done || _terrain_maps_dirty) {
		return; // Wait for the final upload so it is counted.
	}
	for (const Vector2i &c : _bench_chunks) {
		if (!chunk_buffers.has(c)) {
			return;
		}
		Ref<TerrainChunk> chunk = chunk_buffers[c];
		if (chunk.is_null() || chunk->get_state() < TerrainChunk::STATE_VISUAL_ONLY) {
			return;
		}
	}
	_bench_done = true;

	int total_instances = 0;
	uint64_t hash = _bench_content_hash(total_instances);

	double total_ms = (Time::get_singleton()->get_ticks_usec() - _bench_start_usec) / 1000.0;
	uint64_t frames = Engine::get_singleton()->get_process_frames() - _bench_first_frame + 1;

	// Per-frame timeline (chronological) and percentiles (sorted).
	std::vector<uint64_t> frame_keys;
	for (const KeyValue<uint64_t, double> &E : _bench_frame_ms) {
		frame_keys.push_back(E.key);
	}
	std::sort(frame_keys.begin(), frame_keys.end());
	String timeline;
	std::vector<double> per_frame;
	for (uint64_t k : frame_keys) {
		timeline += String::num(_bench_frame_ms[k], 1) + " ";
		per_frame.push_back(_bench_frame_ms[k]);
	}
	std::sort(per_frame.begin(), per_frame.end());
	double main_max = per_frame.empty() ? 0.0 : per_frame.back();
	double main_p95 = per_frame.empty() ? 0.0 : per_frame[(size_t)((per_frame.size() - 1) * 0.95)];
	UtilityFunctions::print("[BENCH] main-thread ms per frame: ", timeline);

	UtilityFunctions::print(
			"[BENCH] total_ms=", String::num(total_ms, 1), " main_max_ms=", String::num(main_max, 1),
			" main_p95_ms=", String::num(main_p95, 1), " math_ms=", String::num(_bench_math_ms, 1),
			" scatter_ms=", String::num(_bench_scatter_ms, 1), " upload_ms=", String::num(_bench_upload_ms, 1),
			" flush_ms=", String::num(_bench_flush_ms, 1), "/", _bench_flushes,
			" collision_ms=", String::num(_bench_collision_ms, 1), " make_ms=", String::num(_bench_make_ms, 1),
			" finalize_ms=", String::num(_bench_finalize_ms, 1), " frames=", (int64_t)frames,
			" chunks=", (int)_bench_chunks.size(), " instances=", total_instances,
			" hash=", String::num_uint64(hash, 16)
	);

	if (get_tree()) {
		get_tree()->quit();
	}
}

} // namespace godot
