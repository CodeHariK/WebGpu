#include "instanced_group.h"

#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

FolioInstancedGroup::FolioInstancedGroup() {}

FolioInstancedGroup::~FolioInstancedGroup() {}

void FolioInstancedGroup::build(
		const Ref<Mesh> &p_mesh,
		const TypedArray<Transform3D> &p_transforms
) {
	if (p_mesh.is_null()) {
		return;
	}
	const int count = p_transforms.size();

	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(p_mesh);
	mm->set_instance_count(count);
	for (int i = 0; i < count; i++) {
		mm->set_instance_transform(i, (Transform3D)p_transforms[i]);
	}

	if (!mmi) {
		mmi = memnew(MultiMeshInstance3D);
		mmi->set_name("Instances");
		add_child(mmi);
	}
	mmi->set_multimesh(mm);
}

int FolioInstancedGroup::get_count() const {
	if (mmi && mmi->get_multimesh().is_valid()) {
		return mmi->get_multimesh()->get_instance_count();
	}
	return 0;
}

void FolioInstancedGroup::_bind_methods() {
	ClassDB::bind_method(D_METHOD("build", "mesh", "transforms"), &FolioInstancedGroup::build);
	ClassDB::bind_method(D_METHOD("get_count"), &FolioInstancedGroup::get_count);
}
