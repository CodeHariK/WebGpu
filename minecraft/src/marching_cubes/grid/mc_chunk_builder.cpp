// MCChunkBuilder: merge placed tile meshes into one ArrayMesh + collider faces (see header).
#include "marching_cubes/grid/mc_chunk_builder.h"

namespace godot {

// Surface arrays of p_mesh, read from the RenderingServer only the first time.
const std::vector<Array> &MCSurfaceCache::get(const Ref<Mesh> &p_mesh) {
	auto it = cache.find(p_mesh.ptr());
	if (it != cache.end()) {
		return it->second;
	}
	std::vector<Array> arrays;
	for (int s = 0; s < p_mesh->get_surface_count(); s++) {
		arrays.push_back(p_mesh->surface_get_arrays(s));
	}
	return cache.emplace(p_mesh.ptr(), std::move(arrays)).first->second;
}

// The surface for p_material, created on first use (surfaces keep first-seen order).
MCChunkBuilder::Surface &MCChunkBuilder::_surface_for(const Ref<Material> &p_material) {
	for (Surface &surface : surfaces) {
		if (surface.material == p_material) {
			return surface;
		}
	}
	surfaces.emplace_back();
	surfaces.back().material = p_material;
	return surfaces.back();
}

namespace {

// Grow an optional array from p_old to p_new entries; p_fill(i) gives the value for entry i of
// the new tile (i < p_count), entries before p_old_base (earlier tiles without it) get p_default.
// Everything goes through ptrw(): Packed*Array::set / push_back are one engine call per element.
template <
		typename TArray,
		typename TValue>
TValue *
grow(TArray &r_array,
	 int64_t p_old_base,
	 int64_t p_new_size,
	 const TValue &p_default) {
	int64_t old = r_array.size();
	r_array.resize(p_new_size);
	TValue *w = r_array.ptrw();
	for (int64_t i = old; i < p_old_base; i++) {
		w[i] = p_default;
	}
	return w + p_old_base;
}

} // namespace

// Append one tile surface moved by p_xform: positions, normals / tangents (turned), colours,
// UVs, indices shifted past the vertices already there, and the triangles for the collider.
void MCChunkBuilder::_append(
		Surface &r_surface,
		const Array &p_arrays,
		const Transform3D &p_xform
) {
	const PackedVector3Array src_v = p_arrays[Mesh::ARRAY_VERTEX];
	if (src_v.is_empty()) {
		return;
	}
	const PackedVector3Array src_n = p_arrays[Mesh::ARRAY_NORMAL];
	const PackedFloat32Array src_t = p_arrays[Mesh::ARRAY_TANGENT];
	const PackedColorArray src_c = p_arrays[Mesh::ARRAY_COLOR];
	const PackedVector2Array src_uv = p_arrays[Mesh::ARRAY_TEX_UV];
	const PackedVector2Array src_uv2 = p_arrays[Mesh::ARRAY_TEX_UV2];
	const PackedInt32Array src_i = p_arrays[Mesh::ARRAY_INDEX];

	const int64_t base = r_surface.vertices.size();
	const int64_t count = src_v.size();
	const Basis normal_basis = p_xform.basis.inverse().transposed(); // correct for any scale

	// Positions.
	r_surface.vertices.resize(base + count);
	Vector3 *wv = r_surface.vertices.ptrw() + base;
	const Vector3 *rv = src_v.ptr();
	for (int64_t i = 0; i < count; i++) {
		wv[i] = p_xform.xform(rv[i]);
	}

	// Optional arrays: once any tile has one, every vertex gets a value.
	if (r_surface.has_normals || !src_n.is_empty()) {
		r_surface.has_normals = true;
		Vector3 *w = grow(r_surface.normals, base, base + count, Vector3(0, 1, 0));
		const Vector3 *r = src_n.size() == count ? src_n.ptr() : nullptr;
		for (int64_t i = 0; i < count; i++) {
			w[i] = r ? normal_basis.xform(r[i]).normalized() : Vector3(0, 1, 0);
		}
	}
	if (r_surface.has_tangents || !src_t.is_empty()) {
		r_surface.has_tangents = true;
		float *w = grow(r_surface.tangents, base * 4, (base + count) * 4, 0.0f);
		const float *r = src_t.size() == count * 4 ? src_t.ptr() : nullptr;
		for (int64_t i = 0; i < count; i++) {
			Vector3 t = r ? p_xform.basis.xform(Vector3(r[i * 4], r[i * 4 + 1], r[i * 4 + 2])).normalized()
						  : Vector3(1, 0, 0);
			w[i * 4] = t.x;
			w[i * 4 + 1] = t.y;
			w[i * 4 + 2] = t.z;
			w[i * 4 + 3] = r ? r[i * 4 + 3] : 1.0f;
		}
	}
	if (r_surface.has_colors || !src_c.is_empty()) {
		r_surface.has_colors = true;
		Color *w = grow(r_surface.colors, base, base + count, Color(1, 1, 1, 1));
		const Color *r = src_c.size() == count ? src_c.ptr() : nullptr;
		for (int64_t i = 0; i < count; i++) {
			w[i] = r ? r[i] : Color(1, 1, 1, 1);
		}
	}
	if (r_surface.has_uvs || !src_uv.is_empty()) {
		r_surface.has_uvs = true;
		Vector2 *w = grow(r_surface.uvs, base, base + count, Vector2());
		const Vector2 *r = src_uv.size() == count ? src_uv.ptr() : nullptr;
		for (int64_t i = 0; i < count; i++) {
			w[i] = r ? r[i] : Vector2();
		}
	}
	if (r_surface.has_uv2s || !src_uv2.is_empty()) {
		r_surface.has_uv2s = true;
		Vector2 *w = grow(r_surface.uv2s, base, base + count, Vector2());
		const Vector2 *r = src_uv2.size() == count ? src_uv2.ptr() : nullptr;
		for (int64_t i = 0; i < count; i++) {
			w[i] = r ? r[i] : Vector2();
		}
	}

	// Indices (shifted), and the same triangles for the collider. Tiles are only turned (never
	// mirrored) in Godot, so the winding stays the same.
	const int64_t index_count = src_i.is_empty() ? count : src_i.size();
	const int32_t *ri = src_i.is_empty() ? nullptr : src_i.ptr();
	const int64_t index_base = r_surface.indices.size();
	const int64_t face_base = faces.size();
	r_surface.indices.resize(index_base + index_count);
	faces.resize(face_base + index_count);
	int32_t *wi = r_surface.indices.ptrw() + index_base;
	Vector3 *wf = faces.ptrw() + face_base;
	for (int64_t i = 0; i < index_count; i++) {
		int32_t vi = ri ? ri[i] : static_cast<int32_t>(i);
		wi[i] = static_cast<int32_t>(base) + vi;
		wf[i] = wv[vi];
	}
}

// Add every triangle surface of p_mesh (cached arrays), each to its material's surface.
void MCChunkBuilder::add(
		const Ref<Mesh> &p_mesh,
		const Transform3D &p_xform,
		MCSurfaceCache &r_cache
) {
	const std::vector<Array> &arrays = r_cache.get(p_mesh);
	const ArrayMesh *array_mesh = Object::cast_to<ArrayMesh>(p_mesh.ptr());
	for (int s = 0; s < static_cast<int>(arrays.size()); s++) {
		if (array_mesh && array_mesh->surface_get_primitive_type(s) != Mesh::PRIMITIVE_TRIANGLES) {
			continue; // tiles are triangle meshes; skip anything else rather than mis-merge it
		}
		_append(_surface_for(p_mesh->surface_get_material(s)), arrays[static_cast<size_t>(s)], p_xform);
	}
}

// Build the merged ArrayMesh: one surface per material, only the arrays some tile had.
Ref<ArrayMesh> MCChunkBuilder::commit() const {
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	for (const Surface &surface : surfaces) {
		if (surface.vertices.is_empty()) {
			continue;
		}
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = surface.vertices;
		if (surface.has_normals) {
			arrays[Mesh::ARRAY_NORMAL] = surface.normals;
		}
		if (surface.has_tangents) {
			arrays[Mesh::ARRAY_TANGENT] = surface.tangents;
		}
		if (surface.has_colors) {
			arrays[Mesh::ARRAY_COLOR] = surface.colors;
		}
		if (surface.has_uvs) {
			arrays[Mesh::ARRAY_TEX_UV] = surface.uvs;
		}
		if (surface.has_uv2s) {
			arrays[Mesh::ARRAY_TEX_UV2] = surface.uv2s;
		}
		arrays[Mesh::ARRAY_INDEX] = surface.indices;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		mesh->surface_set_material(mesh->get_surface_count() - 1, surface.material);
	}
	return mesh;
}

} // namespace godot
