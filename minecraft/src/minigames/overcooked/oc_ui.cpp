#include "oc_ui.h"
#include "oc_manager.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// --- OCOrderUI Implementation ---

void OCOrderUI::_bind_methods() {}

OCOrderUI::OCOrderUI() {}
OCOrderUI::~OCOrderUI() {}

void OCOrderUI::_ready() {
	CUI::_ready();

	// Main container for orders
	Panel *main_panel = add_panel(this, "OrderPanel", Control::PRESET_TOP_LEFT, Vector2(250, 400));
	main_panel->set_position(Vector2(20, 20));

	VBoxContainer *main_vbox = add_vbox(main_panel, "MainVBox");
	main_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 10);

	score_label = add_label(main_vbox, "Score: 0", "ScoreLabel");

	add_label(main_vbox, "ACTIVE ORDERS", "Title");

	order_list = add_vbox(main_vbox, "OrderList");
	order_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	rebuild_ui();
}

void OCOrderUI::_exit_tree() {}

void OCOrderUI::_process(double delta) {
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (!om || !order_list)
		return;

	const std::vector<Ref<OCRecipe>> &orders = om->get_active_orders();

	// 1. Rebuild if count changed
	if ((int)orders.size() != order_list->get_child_count()) {
		rebuild_ui();
	}

	// 2. Update Progress Bars and Score
	score_label->set_text("Score: " + String::num_int64(om->get_score()));

	for (int i = 0; i < (int)orders.size(); i++) {
		Node *child = order_list->get_child(i);
		ProgressBar *bar = Object::cast_to<ProgressBar>(child->find_child("Timer", true, false));
		if (bar) {
			bar->set_max(orders[i]->get_base_time());
			bar->set_value(orders[i]->get_time_remaining());
		}
	}
}

void OCOrderUI::rebuild_ui() {
	if (!order_list)
		return;

	// Clear existing
	for (int i = order_list->get_child_count() - 1; i >= 0; i--) {
		Node *child = order_list->get_child(i);
		order_list->remove_child(child);
		child->queue_free();
	}

	OvercookedManager *om = OvercookedManager::get_singleton();
	if (!om)
		return;

	const std::vector<Ref<OCRecipe>> &orders = om->get_active_orders();
	for (int i = 0; i < (int)orders.size(); i++) {
		const Ref<OCRecipe> &order = orders[i];
		String order_id = "Order_" + String::num_int64(i);

		// Use standard CUI panel for row background
		Panel *panel = add_panel(order_list, order_id + "_BG", Control::PRESET_FULL_RECT, Vector2(210, 60));
		panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);

		VBoxContainer *row_vbox = add_vbox(panel, order_id + "_VBox");
		row_vbox->set_offset(Side::SIDE_LEFT, 5);
		row_vbox->set_offset(Side::SIDE_RIGHT, -5);

		add_label(row_vbox, order->get_dish_name(), order_id + "_Label");

		ProgressBar *timer_bar = add_progress_bar(row_vbox, "Timer");
		timer_bar->set_show_percentage(false);
		timer_bar->set_custom_minimum_size(Vector2(200, 10));
	}
}

// --- OCRecipeEditorUI Implementation ---

void OCRecipeEditorUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_add_ingredient_pressed"), &OCRecipeEditorUI::_on_add_ingredient_pressed);
	ClassDB::bind_method(D_METHOD("_on_save_recipe_pressed"), &OCRecipeEditorUI::_on_save_recipe_pressed);
	ClassDB::bind_method(D_METHOD("_on_add_inventory_item_pressed"), &OCRecipeEditorUI::_on_add_inventory_item_pressed);
	ClassDB::bind_method(D_METHOD("_on_save_inventory_pressed"), &OCRecipeEditorUI::_on_save_inventory_pressed);
}

OCRecipeEditorUI::OCRecipeEditorUI() {}
OCRecipeEditorUI::~OCRecipeEditorUI() {}

void OCRecipeEditorUI::_ready() {
	CUI::_ready();

	// Main Panel
	Vector2 screen_size = get_viewport_rect().size;
	Vector2 panel_size = Vector2(screen_size.x * 0.7f, screen_size.y * 0.8f);
	Panel *main_panel = add_panel(this, "AdminPanel", PRESET_CENTER, panel_size);

	VBoxContainer *main_vbox = memnew(VBoxContainer);
	main_vbox->set_name("MainVBox");
	main_panel->add_child(main_vbox);
	main_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 20);

	add_label(main_vbox, "ADMIN CONSOLE", "Title");

	TabContainer *tabs = add_tab_container(main_vbox, "AdminTabs");
	tabs->set_v_size_flags(SIZE_EXPAND_FILL);

	// --- RECIPE TAB ---
	VBoxContainer *recipe_tab = memnew(VBoxContainer);
	recipe_tab->set_name("Recipes");
	tabs->add_child(recipe_tab);

	// Name
	HBoxContainer *name_row = memnew(HBoxContainer);
	recipe_tab->add_child(name_row);
	add_label(name_row, "Dish Name: ", "NameLabel");
	name_edit = add_line_edit(name_row, "e.g. Burger", "NameEdit");
	name_edit->set_h_size_flags(SIZE_EXPAND_FILL);

	// Points & Time
	HBoxContainer *settings_row = memnew(HBoxContainer);
	recipe_tab->add_child(settings_row);
	add_label(settings_row, "Points: ", "PointsLabel");
	points_spin = add_spin_box(settings_row, 10, 1000, 10, 100, "PointsSpin");
	add_label(settings_row, "Time: ", "TimeLabel");
	time_spin = add_spin_box(settings_row, 10, 300, 5, 60, "TimeSpin");

	add_label(recipe_tab, "Ingredients:", "IngredientsHeader");
	ScrollContainer *scroll = add_scroll(recipe_tab, "RecipeScroll");
	scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	ingredients_container = add_vbox(scroll, "IngredientsList");

	add_button(recipe_tab, "Add Ingredient", Callable(this, "_on_add_ingredient_pressed"), "AddIngBtn");
	add_button(recipe_tab, "SAVE RECIPE", Callable(this, "_on_save_recipe_pressed"), "SaveRecipeBtn");

	// --- INVENTORY TAB ---
	VBoxContainer *inv_tab = memnew(VBoxContainer);
	inv_tab->set_name("Inventory");
	tabs->add_child(inv_tab);

	add_label(inv_tab, "Manage Stock & Items", "InvHeader");
	ScrollContainer *inv_scroll = add_scroll(inv_tab, "InvScroll");
	inv_scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	inventory_container = add_vbox(inv_scroll, "InventoryList");

	add_button(inv_tab, "Add New Item", Callable(this, "_on_add_inventory_item_pressed"), "AddInvBtn");
	add_button(inv_tab, "SAVE INVENTORY", Callable(this, "_on_save_inventory_pressed"), "SaveInvBtn");

	// Initial population - delay by one frame or check singleton
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (om) {
		_on_add_ingredient_pressed();
		rebuild_inventory_ui();
	}
}

void OCRecipeEditorUI::rebuild_inventory_ui() {
	// Clear current rows
	for (int i = inventory_container->get_child_count() - 1; i >= 0; i--) {
		inventory_container->get_child(i)->queue_free();
	}

	Dictionary inv = OvercookedManager::get_singleton()->get_inventory_node()->get_items();
	Array keys = inv.keys();
	for (int i = 0; i < keys.size(); i++) {
		String name = keys[i];
		int qty = (int)inv[name];

		HBoxContainer *row = memnew(HBoxContainer);
		inventory_container->add_child(row);

		LineEdit *name_edit_row = add_line_edit(row, name, "ItemName");
		name_edit_row->set_text(name);
		name_edit_row->set_h_size_flags(SIZE_EXPAND_FILL);

		SpinBox *qty_spin = add_spin_box(row, 0, 9999, 1, qty, "ItemQty");

		add_button(row, "X", Callable(row, "queue_free"), "DelInvBtn");
	}
}

void OCRecipeEditorUI::_on_add_inventory_item_pressed() {
	HBoxContainer *row = memnew(HBoxContainer);
	inventory_container->add_child(row);

	LineEdit *name_edit_row = add_line_edit(row, "New Item", "ItemName");
	name_edit_row->set_h_size_flags(SIZE_EXPAND_FILL);

	SpinBox *qty_spin = add_spin_box(row, 0, 9999, 1, 100, "ItemQty");

	add_button(row, "X", Callable(row, "queue_free"), "DelInvBtn");
}

void OCRecipeEditorUI::_on_save_inventory_pressed() {
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (!om || !om->get_inventory_node())
		return;

	Dictionary new_inv;
	for (int i = 0; i < inventory_container->get_child_count(); i++) {
		HBoxContainer *row = Object::cast_to<HBoxContainer>(inventory_container->get_child(i));
		if (row) {
			LineEdit *name_edit_row = Object::cast_to<LineEdit>(row->get_child(0));
			SpinBox *qty_spin = Object::cast_to<SpinBox>(row->get_child(1));
			if (name_edit_row && qty_spin) {
				new_inv[name_edit_row->get_text()] = (int)qty_spin->get_value();
			}
		}
	}
	om->get_inventory_node()->set_items(new_inv);
	om->save_inventory();
	UtilityFunctions::print("Inventory Saved.");
}

void OCRecipeEditorUI::_on_add_ingredient_pressed() {
	HBoxContainer *row = memnew(HBoxContainer);
	ingredients_container->add_child(row);

	OptionButton *type_opt = add_option_button(row, "Type");
	type_opt->set_h_size_flags(SIZE_EXPAND_FILL);

	Dictionary inv;
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (om && om->get_inventory_node()) {
		inv = om->get_inventory_node()->get_items();
	}
	Array keys = inv.keys();
	for (int i = 0; i < keys.size(); i++) {
		type_opt->add_item(keys[i], i);
	}

	OptionButton *state_opt = add_option_button(row, "State");
	state_opt->add_item("RAW", 0);
	state_opt->add_item("CHOPPED", 1);
	state_opt->add_item("COOKED", 2);
	state_opt->add_item("BLENDED", 3);
	state_opt->add_item("FROZEN", 4);
	state_opt->add_item("BURNT", 5);
	state_opt->select(0);

	add_button(row, "X", Callable(row, "queue_free"), "DelBtn");
}

void OCRecipeEditorUI::_on_save_recipe_pressed() {
	Ref<OCRecipe> recipe;
	recipe.instantiate();
	recipe->set_dish_name(name_edit->get_text());
	recipe->set_points((int)points_spin->get_value());
	recipe->set_base_time((float)time_spin->get_value());

	std::vector<OCRecipeRequirement> requirements;

	for (int i = 0; i < ingredients_container->get_child_count(); i++) {
		HBoxContainer *row = Object::cast_to<HBoxContainer>(ingredients_container->get_child(i));
		if (row) {
			OptionButton *type_opt = Object::cast_to<OptionButton>(row->get_child(0));
			OptionButton *state_opt = Object::cast_to<OptionButton>(row->get_child(1));
			if (type_opt && state_opt) {
				OCRecipeRequirement req;
				req.type = type_opt->get_item_text(type_opt->get_selected());
				req.state = state_opt->get_selected();
				requirements.push_back(req);
			}
		}
	}
	recipe->set_requirements(requirements);

	OvercookedManager::get_singleton()->save_recipe_to_json(recipe);
	UtilityFunctions::print("Recipe Saved: ", recipe->get_dish_name());
}

} // namespace godot
