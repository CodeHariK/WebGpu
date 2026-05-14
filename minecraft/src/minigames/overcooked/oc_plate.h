#ifndef OC_PLATE_H
#define OC_PLATE_H

#include "oc_ingredient.h"
#include <vector>

namespace godot {

class OCPlate : public OCIngredient {
	GDCLASS(OCPlate, OCIngredient)

private:
	std::vector<OCIngredient*> contents;
	Node3D* content_parent = nullptr;

protected:
	static void _bind_methods();

public:
	OCPlate();
	~OCPlate();

	void _ready() override;
	void _process(double delta) override;
	
	bool add_ingredient(OCIngredient* p_ing);
	const std::vector<OCIngredient*>& get_contents() const { return contents; }
	
	void clear_contents();
};

} // namespace godot

#endif // OC_PLATE_H
