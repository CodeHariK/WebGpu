#include "oc_order_ui.h"
#include "oc_manager.h"
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void OCOrderUI::_bind_methods() {}

OCOrderUI::OCOrderUI() {}
OCOrderUI::~OCOrderUI() {}

void OCOrderUI::_ready() {
	// Create basic layout
	VBoxContainer *main_vbox = memnew(VBoxContainer);
	main_vbox->set_anchors_and_offsets_preset(Control::PRESET_TOP_LEFT, Control::PRESET_MODE_MINSIZE, 20);
	add_child(main_vbox);

	score_label = memnew(Label);
	score_label->set_text("Score: 0");
	main_vbox->add_child(score_label);

	Label *orders_header = memnew(Label);
	orders_header->set_text("ACTIVE ORDERS:");
	main_vbox->add_child(orders_header);

	order_list = memnew(VBoxContainer);
	main_vbox->add_child(order_list);

	// Register with Manager
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (om) {
		om->register_ui(this);
	}
}

void OCOrderUI::_exit_tree() {
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (om) {
		om->unregister_ui(this);
	}
}

void OCOrderUI::_process(double delta) {
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (!om || !order_list) return;

	const std::vector<OCOrder>& orders = om->get_active_orders();

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
			bar->set_max(orders[i].total_time);
			bar->set_value(orders[i].time_remaining);
		}
	}
}

void OCOrderUI::rebuild_ui() {
	if (!order_list) return;

	// Clear existing
	for (int i = order_list->get_child_count() - 1; i >= 0; i--) {
		Node *child = order_list->get_child(i);
		order_list->remove_child(child);
		child->queue_free();
	}

	OvercookedManager *om = OvercookedManager::get_singleton();
	if (!om) return;

	const std::vector<OCOrder>& orders = om->get_active_orders();
	for (const auto& order : orders) {
		PanelContainer *panel = memnew(PanelContainer);
		order_list->add_child(panel);

		VBoxContainer *row_vbox = memnew(VBoxContainer);
		panel->add_child(row_vbox);

		Label *name_label = memnew(Label);
		name_label->set_text(order.dish_name);
		row_vbox->add_child(name_label);

		ProgressBar *timer_bar = memnew(ProgressBar);
		timer_bar->set_name("Timer");
		timer_bar->set_show_percentage(false);
		timer_bar->set_custom_minimum_size(Vector2(200, 10));
		row_vbox->add_child(timer_bar);
	}
}

} // namespace godot
