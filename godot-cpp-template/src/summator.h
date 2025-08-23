#ifndef SUMMATOR_CLASS_H
#define SUMMATOR_CLASS_H

#include <godot_cpp/classes/ref.hpp>

using namespace godot;

class SummatorClass : public RefCounted {
	GDCLASS(SummatorClass, RefCounted);

	int count;

protected:
	static void _bind_methods();

public:
	SummatorClass();
	~SummatorClass();

	void print_type(const Variant &p_variant) const;
	void add(int p_value);
	void sub(int p_value);
	void mul(int p_value);
	void reset();
	int get_total() const;
};

#endif // SUMMATOR_CLASS_H
