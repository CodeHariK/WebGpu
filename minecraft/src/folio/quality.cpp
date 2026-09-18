#include "quality.h"

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

FolioQuality::FolioQuality() {}

FolioQuality::~FolioQuality() {}

void FolioQuality::_ready() {
	events.instantiate();

	// Mobile defaults to the low tier (folio: userAgent sniff -> level 1).
	const bool is_mobile = OS::get_singleton() && OS::get_singleton()->has_feature("mobile");
	level = is_mobile ? 1 : 0;
}

void FolioQuality::change_level(int p_level) {
	if (p_level == level) {
		return;
	}
	level = p_level;

	if (events.is_valid()) {
		Array args;
		args.push_back(level);
		events->trigger("change", args);
	}
}

void FolioQuality::_bind_methods() {
	ClassDB::bind_method(D_METHOD("change_level", "level"), &FolioQuality::change_level, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_level"), &FolioQuality::get_level);
	ClassDB::bind_method(D_METHOD("get_events"), &FolioQuality::get_events);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "level"), "change_level", "get_level");
}

} // namespace godot
