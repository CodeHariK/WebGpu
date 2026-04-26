#ifndef BLACKBOARD_H
#define BLACKBOARD_H

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

class Blackboard : public RefCounted {
	GDCLASS(Blackboard, RefCounted)

private:
	Dictionary data;

protected:
	static void _bind_methods();

public:
	Blackboard();
	~Blackboard();

	void set_value(const String &p_key, const Variant &p_value);
	Variant get_value(const String &p_key, const Variant &p_default = Variant()) const;
	bool has_value(const String &p_key) const;
	void erase_value(const String &p_key);
	void clear();
};

} // namespace godot

#endif // BLACKBOARD_H
