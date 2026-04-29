#ifndef OVERCOOKED_MANAGER_H
#define OVERCOOKED_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

class CounterStation;
class Ingredient;

class OvercookedManager : public Node {
	GDCLASS(OvercookedManager, Node)

private:
	static OvercookedManager *singleton;
	std::vector<CounterStation *> stations;
	std::vector<Ingredient *> ingredients;

protected:
	static void _bind_methods();

public:
	OvercookedManager();
	~OvercookedManager();

	static OvercookedManager *get_singleton();

	void register_station(CounterStation *p_station);
	void unregister_station(CounterStation *p_station);

	void register_ingredient(Ingredient *p_ing);
	void unregister_ingredient(Ingredient *p_ing);

	CounterStation *get_closest_station(const Vector3 &p_from, float p_max_dist);
	Ingredient *get_closest_ingredient(const Vector3 &p_from, float p_max_dist);
};

} // namespace godot

#endif // OVERCOOKED_MANAGER_H
