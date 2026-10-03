#include "tuning_panel.h"

#include "cui.h"
#include "tuning_section.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/window.hpp>

namespace godot {

TuningPanel *TuningPanel::singleton = nullptr;
int64_t TuningPanel::next_id = 1;

static const Vector2 PANEL_SIZE = Vector2(540, 600);
static const int PANEL_LAYER = 90; // above gameplay HUDs, below modal dialogs

void TuningPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("toggle"), &TuningPanel::toggle);
	ClassDB::bind_method(D_METHOD("is_open"), &TuningPanel::is_open);
	ClassDB::bind_method(D_METHOD("save_all"), &TuningPanel::save_all);
	ClassDB::bind_method(D_METHOD("load_all"), &TuningPanel::load_all);
}

TuningPanel::TuningPanel() {}

TuningPanel::~TuningPanel() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

TuningPanel *TuningPanel::ensure() {
	if (singleton) {
		return singleton;
	}
	SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
	if (!tree || !tree->get_root()) {
		return nullptr;
	}
	TuningPanel *p = memnew(TuningPanel);
	p->set_name("TuningPanel");
	p->set_layer(PANEL_LAYER);
	p->_build();
	singleton = p;
	// Deferred: callers are usually mid-_ready, when the root is busy adding children.
	tree->get_root()->call_deferred("add_child", p);
	return p;
}

void TuningPanel::_build() {
	ui = memnew(CUI);
	add_child(ui);
	ui->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	ui->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	Button *btn = ui->add_button(ui, "Tuning", Callable(this, "toggle"), "tuning_toggle");
	btn->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT, Control::PRESET_MODE_MINSIZE, 20);

	panel = ui->add_panel(ui, "tuning_panel", Control::PRESET_TOP_LEFT, PANEL_SIZE);
	panel->set_position(Vector2(panel->get_position().x, 50));
	panel->set_visible(false);

	VBoxContainer *col = ui->add_vbox(panel, "tuning_col", 6);
	col->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 10);

	tabs = ui->add_tab_container(col, "tuning_tabs");
	tabs->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	HBoxContainer *buttons = ui->add_hbox(col, "tuning_buttons", 8);
	ui->add_button(buttons, "Save", Callable(this, "save_all"));
	ui->add_button(buttons, "Load", Callable(this, "load_all"));
}

// --- Sections ----------------------------------------------------------------

int64_t TuningPanel::add_section(TuningSection *p_section) {
	int64_t id = next_id++;
	sections[id] = p_section;

	String key = vformat("tune_%d", id);
	ScrollContainer *scroll = ui->add_scroll(tabs, key);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	VBoxContainer *body = ui->add_vbox(scroll, key + "_body", 2);
	body->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	p_section->build(ui, body, id);

	tab_ids[id] = scroll->get_instance_id();
	int idx = _tab_index(id);
	if (idx >= 0) {
		tabs->set_tab_title(idx, p_section->get_title());
	}
	return id;
}

void TuningPanel::remove_section(int64_t p_id) {
	sections.erase(p_id);
	if (tab_ids.has(p_id)) {
		Node *tab = Object::cast_to<Node>(ObjectDB::get_instance(tab_ids[p_id]));
		if (tab) {
			tab->get_parent()->remove_child(tab);
			tab->queue_free();
		}
		tab_ids.erase(p_id);
	}
}

void TuningPanel::set_section_shown(
		int64_t p_id,
		bool p_shown
) {
	int idx = _tab_index(p_id);
	if (idx >= 0 && tabs->is_tab_hidden(idx) == p_shown) {
		tabs->set_tab_hidden(idx, !p_shown);
	}
}

int TuningPanel::_tab_index(int64_t p_id) const {
	HashMap<int64_t, uint64_t>::ConstIterator it = tab_ids.find(p_id);
	if (it == tab_ids.end() || !tabs) {
		return -1;
	}
	Control *tab = Object::cast_to<Control>(ObjectDB::get_instance(it->value));
	return tab ? tabs->get_tab_idx_from_control(tab) : -1;
}

// --- Panel actions -----------------------------------------------------------

void TuningPanel::toggle() {
	if (!panel) {
		return;
	}
	bool open = !panel->is_visible();
	panel->set_visible(open);
	if (open) {
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	}
}

bool TuningPanel::is_open() const { return panel && panel->is_visible(); }

void TuningPanel::save_all() {
	for (KeyValue<int64_t, TuningSection *> &kv : sections) {
		kv.value->save();
	}
}

void TuningPanel::load_all() {
	for (KeyValue<int64_t, TuningSection *> &kv : sections) {
		kv.value->load();
	}
}

void TuningPanel::_on_slider(
		double p_value,
		int64_t p_id,
		int p_row
) {
	HashMap<int64_t, TuningSection *>::Iterator it = sections.find(p_id);
	if (it != sections.end()) {
		it->value->on_slider(p_row, (float)p_value);
	}
}

} // namespace godot
