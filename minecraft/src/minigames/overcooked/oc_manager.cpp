#include "oc_manager.h"
#include "oc_ingredient.h"
#include "oc_station.h"
#include "oc_recipe.h"
#include "oc_ui.h"
#include "oc_plate.h"
#include "oc_interactor.h"
#include "../../game_manager/game_manager.h"
#include <godot_cpp/classes/marker3d.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <algorithm>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

OvercookedManager *OvercookedManager::singleton = nullptr;

void OvercookedManager::load_recipes_from_dir() {
	if (!DirAccess::dir_exists_absolute(recipe_dir)) {
		DirAccess::make_dir_recursive_absolute(recipe_dir);
		return;
	}

	Ref<DirAccess> dir = DirAccess::open(recipe_dir);
	if (dir.is_null()) return;

	dir->list_dir_begin();
	String file_name = dir->get_next();

	while (file_name != "") {
		if (!dir->current_is_dir() && file_name.ends_with(".json")) {
			String full_path = recipe_dir + file_name;
			String json_string = FileAccess::get_file_as_string(full_path);
			
			Ref<JSON> json;
			json.instantiate();
			if (json->parse(json_string) == OK) {
				Dictionary dict = json->get_data();
				Ref<OCRecipe> recipe;
				recipe.instantiate();
				recipe->from_dict(dict);
				available_recipes.push_back(recipe);
				UtilityFunctions::print("OCManager: Loaded recipe ", recipe->get_dish_name(), " from ", file_name);
			}
		}
		file_name = dir->get_next();
	}
}

void OvercookedManager::save_recipe_to_json(Ref<OCRecipe> p_recipe) {
	if (p_recipe.is_null()) return;

	if (!DirAccess::dir_exists_absolute(recipe_dir)) {
		DirAccess::make_dir_recursive_absolute(recipe_dir);
	}

	String file_name = p_recipe->get_dish_name().to_lower().replace(" ", "_") + ".json";
	String full_path = recipe_dir + file_name;

	String json_string = JSON::stringify(p_recipe->to_dict(), "\t");
	Ref<FileAccess> file = FileAccess::open(full_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(json_string);
		UtilityFunctions::print("OCManager: Saved recipe to ", full_path);
	}
}

void OvercookedManager::load_inventory() {
	if (FileAccess::file_exists(inventory_path)) {
		String json_string = FileAccess::get_file_as_string(inventory_path);
		inventory->from_json(json_string);
		UtilityFunctions::print("OCManager: Inventory loaded.");
		return;
	}

	// Default inventory if none exists
	inventory->add_item("Tomato", 100);
	inventory->add_item("Lettuce", 100);
	inventory->add_item("Onion", 100);
	inventory->add_item("Beef", 50);
	save_inventory();
}

void OvercookedManager::save_inventory() {
	String json_string = inventory->to_json();
	Ref<FileAccess> file = FileAccess::open(inventory_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(json_string);
	}
}

bool OvercookedManager::has_inventory(const String &p_type, int p_amount) const {
	return inventory->has_item(p_type, p_amount);
}

bool OvercookedManager::try_consume_inventory(const String &p_type, int p_amount) {
	if (inventory->try_consume(p_type, p_amount)) {
		save_inventory();
		return true;
	}
	return false;
}

void OvercookedManager::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) return;

	singleton = this;

	if (inventory == nullptr) {
		inventory = memnew(Inventory);
		inventory->set_name("Inventory");
		add_child(inventory);
	}

	// Load runtime recipes first
	load_recipes_from_dir();
	load_inventory();

	// Find UI components
	Node *root = get_tree()->get_current_scene();
	if (root) {
		order_ui = Object::cast_to<OCOrderUI>(root->find_child("OCOrderUI", true, false));
	}

	// NEW: Attach interactor to player manually
	call_deferred("attach_interactor_to_player");
}

void OvercookedManager::attach_interactor_to_player() {
	GameManager *gm = GameManager::get_singleton();
	if (!gm) return;

	Node3D *player = Object::cast_to<Node3D>(gm->get_celeste_controller());
	if (!player) {
		// Retry if player isn't spawned yet
		call_deferred("attach_interactor_to_player");
		return;
	}

	// Only attach Interactor if it doesn't exist
	OCInteractor *interactor = Object::cast_to<OCInteractor>(player->find_child("OCInteractor"));
	if (!interactor) {
		interactor = memnew(OCInteractor);
		interactor->set_name("OCInteractor");
		player->add_child(interactor);
		UtilityFunctions::print("OvercookedManager: Attached Interactor to player.");
	}
}

void OvercookedManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("submit_ingredient", "ingredient"), &OvercookedManager::submit_ingredient);
	ClassDB::bind_method(D_METHOD("spawn_random_order"), &OvercookedManager::spawn_random_order);
	ClassDB::bind_method(D_METHOD("get_score"), &OvercookedManager::get_score);
	
	ClassDB::bind_method(D_METHOD("set_available_recipes", "recipes"), &OvercookedManager::set_available_recipes);
	ClassDB::bind_method(D_METHOD("get_available_recipes"), &OvercookedManager::get_available_recipes);
	
	ClassDB::bind_method(D_METHOD("get_inventory_node"), &OvercookedManager::get_inventory_node);
	ClassDB::bind_method(D_METHOD("attach_interactor_to_player"), &OvercookedManager::attach_interactor_to_player);
	ClassDB::bind_method(D_METHOD("get_needed_ingredients"), &OvercookedManager::get_needed_ingredients);
	ClassDB::bind_method(D_METHOD("create_ingredient"), &OvercookedManager::create_ingredient);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "available_recipes", PROPERTY_HINT_RESOURCE_TYPE, "OCRecipe"), "set_available_recipes", "get_available_recipes");
}

OvercookedManager::OvercookedManager() {}

OvercookedManager::~OvercookedManager() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

OvercookedManager *OvercookedManager::get_singleton() {
	return singleton;
}

void OvercookedManager::register_station(OCStation *p_station) {
	if (std::find(stations.begin(), stations.end(), p_station) == stations.end()) {
		stations.push_back(p_station);
	}
}

void OvercookedManager::unregister_station(OCStation *p_station) {
	auto it = std::find(stations.begin(), stations.end(), p_station);
	if (it != stations.end()) {
		stations.erase(it);
	}
}

void OvercookedManager::register_ingredient(OCIngredient *p_ing) {
	if (std::find(ingredients.begin(), ingredients.end(), p_ing) == ingredients.end()) {
		ingredients.push_back(p_ing);
	}
}

void OvercookedManager::unregister_ingredient(OCIngredient *p_ing) {
	auto it = std::find(ingredients.begin(), ingredients.end(), p_ing);
	if (it != ingredients.end()) {
		ingredients.erase(it);
	}
}

OCStation *OvercookedManager::get_closest_station(const Vector3 &p_from, float p_max_dist) {
	OCStation *closest = nullptr;
	float min_dist_sq = p_max_dist * p_max_dist;

	for (auto s : stations) {
		if (!s || !s->is_inside_tree())
			continue;

		float dist_sq = p_from.distance_squared_to(s->get_global_position());
		if (dist_sq < min_dist_sq) {
			min_dist_sq = dist_sq;
			closest = s;
		}
	}
	return closest;
}

OCIngredient *OvercookedManager::get_closest_ingredient(const Vector3 &p_from, float p_max_dist) {
	OCIngredient *closest = nullptr;
	float min_dist_sq = p_max_dist * p_max_dist;

	for (auto i : ingredients) {
		if (!i || !i->is_inside_tree() || i->get_is_picked_up())
			continue;

		float dist_sq = p_from.distance_squared_to(i->get_global_position());
		if (dist_sq < min_dist_sq) {
			min_dist_sq = dist_sq;
			closest = i;
		}
	}
	return closest;
}

PackedStringArray OvercookedManager::get_needed_ingredients() const {
	PackedStringArray needed;
	for (const auto &order : active_orders) {
		if (!order.is_valid()) continue;
		
		for (const auto &req : order->get_requirements()) {
			// Add the base type (e.g., if we need Tomato_Chopped, we need a Tomato)
			if (!needed.has(req.type)) {
				needed.append(req.type);
			}
		}
	}
	return needed;
}

OCIngredient *OvercookedManager::create_ingredient(const String &p_type) {
	OCIngredient *ing = memnew(OCIngredient);
	ing->set_ingredient_type(p_type);
	// In the future, we could look up a PackedScene map here for custom models
	return ing;
}

void OvercookedManager::spawn_random_order() {
	if (available_recipes.size() == 0) return;

	int idx = rand() % available_recipes.size();
	Ref<OCRecipe> template_recipe = available_recipes[idx];
	if (!template_recipe.is_valid()) return;

	Ref<OCRecipe> new_order = template_recipe->clone();
	active_orders.push_back(new_order);
	
	UtilityFunctions::print("OCManager: New Order: ", new_order->get_dish_name());

	if (order_ui) {
		order_ui->rebuild_ui();
	}
}

void OvercookedManager::_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint()) return;

	// Spawn orders periodically
	order_timer += (float)p_delta;
	if (order_timer >= next_order_interval) {
		spawn_random_order();
		order_timer = 0.0f;
		next_order_interval = UtilityFunctions::randf_range(15.0, 30.0);
	}

	// Update order timers
	for (int i = (int)active_orders.size() - 1; i >= 0; i--) {
		float remaining = active_orders[i]->get_time_remaining();
		remaining -= (float)p_delta;
		active_orders[i]->set_time_remaining(remaining);

		if (remaining <= 0) {
			UtilityFunctions::print("OCManager: Order Expired: ", active_orders[i]->get_dish_name());
			active_orders.erase(active_orders.begin() + i);
			if (order_ui) order_ui->rebuild_ui();
		}
	}
}

void OvercookedManager::_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid() && key_event->is_pressed() && !key_event->is_echo()) {
		if (key_event->get_keycode() == KEY_K) {
			Node *root = get_tree()->get_current_scene();
			if (root) {
				Control *editor = Object::cast_to<Control>(root->find_child("OCRecipeEditorUI", true, false));
				if (editor) {
					editor->set_visible(!editor->is_visible());
					// Toggle mouse mode
					Input *input = Input::get_singleton();
					if (editor->is_visible()) {
						input->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
					} else {
						input->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
					}
				}
			}
		}
	}
}

bool OvercookedManager::submit_ingredient(OCIngredient *p_ing) {
	if (!p_ing) return false;

	// Collect ingredients to check
	std::vector<OCIngredient*> submitted_items;
	OCPlate *plate = Object::cast_to<OCPlate>(p_ing);
	
	if (plate) {
		submitted_items = plate->get_contents();
	} else {
		submitted_items.push_back(p_ing);
	}

	for (int i = 0; i < (int)active_orders.size(); i++) {
		const auto& reqs = active_orders[i]->get_requirements();
		
		if (reqs.size() != submitted_items.size()) continue;

		// Matching Logic
		std::vector<OCRecipeRequirement> remaining = reqs;
		bool match = true;
		
		for (OCIngredient* item : submitted_items) {
			bool found = false;
			for (auto it = remaining.begin(); it != remaining.end(); ++it) {
				if (it->type == item->get_ingredient_type() && it->state == (int)item->get_state()) {
					remaining.erase(it);
					found = true;
					break;
				}
			}
			if (!found) {
				match = false;
				break;
			}
		}

		if (match && remaining.empty()) {
			// Success!
			score += active_orders[i]->get_points();
			UtilityFunctions::print("OCManager: Order Complete! +", active_orders[i]->get_points(), " pts. Total: ", score);
			
			active_orders.erase(active_orders.begin() + i);
			
			// Consume Ingredients from Inventory
			for (OCIngredient* item : submitted_items) {
				inventory->try_consume(item->get_ingredient_type(), 1);
			}
			save_inventory();

			// If it was a plate, we don't necessarily delete the plate, just clear it?
			// Or delete everything submitted.
			if (plate) {
				plate->clear_contents();
			} else {
				p_ing->queue_free();
			}

			return true;
		}
	}

	return false;
}

} // namespace godot
