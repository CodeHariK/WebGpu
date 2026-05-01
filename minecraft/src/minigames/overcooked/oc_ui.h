#ifndef OC_UI_H
#define OC_UI_H

#include "../../cui/cui.h"
#include "oc_recipe.h"
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace godot {

/**
 * @brief HUD for displaying active orders and current score.
 */
class OCOrderUI : public CUI {
	GDCLASS(OCOrderUI, CUI)

private:
	VBoxContainer *order_list = nullptr;
	Label *score_label = nullptr;

protected:
	static void _bind_methods();

public:
	OCOrderUI();
	~OCOrderUI();

	void rebuild_ui();

	void _ready() override;
	void _process(double delta) override;
	void _exit_tree() override;
};

/**
 * @brief Admin tool for creating recipes and managing global inventory.
 */
class OCRecipeEditorUI : public CUI {
	GDCLASS(OCRecipeEditorUI, CUI)

private:
	VBoxContainer *ingredients_container = nullptr;
	VBoxContainer *inventory_container = nullptr;

	LineEdit *name_edit = nullptr;
	SpinBox *points_spin = nullptr;
	SpinBox *time_spin = nullptr;

	void _on_add_ingredient_pressed();
	void _on_save_recipe_pressed();

	void _on_add_inventory_item_pressed();
	void _on_save_inventory_pressed();
	void rebuild_inventory_ui();

protected:
	static void _bind_methods();

public:
	OCRecipeEditorUI();
	~OCRecipeEditorUI();

	virtual void _ready() override;
};

} // namespace godot

#endif // OC_UI_H
