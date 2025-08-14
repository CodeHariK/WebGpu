#include "summator.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

SummatorClass::SummatorClass() {
	count = 0;
}

SummatorClass::~SummatorClass() {
}

void SummatorClass::print_type(const Variant &p_variant) const {
	print_line(vformat("Type Hello: %d", p_variant.get_type()));
}

void SummatorClass::add(int p_value) {
	count += p_value;
}
void SummatorClass::sub(int p_value) {
	count -= p_value;
}

void SummatorClass::reset() {
	count = 0;
}

int SummatorClass::get_total() const {
	return count;
}

void SummatorClass::_bind_methods() {
	ClassDB::bind_method(D_METHOD("print_type", "variant"), &SummatorClass::print_type);
	ClassDB::bind_method(D_METHOD("add", "value"), &SummatorClass::add, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("sub", "value"), &SummatorClass::sub, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("reset"), &SummatorClass::reset);
	ClassDB::bind_method(D_METHOD("get_total"), &SummatorClass::get_total);
}
