#include "tuning_section.h"

#include "cui.h"
#include "cui_line_graph.h"
#include "tuning_panel.h"

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

TuningSection::~TuningSection() { detach(); }

// --- Description -------------------------------------------------------------

void TuningSection::begin(
		const String &p_title,
		const String &p_cfg_path,
		const String &p_cfg_section,
		ChangeFn p_on_change
) {
	detach();
	title = p_title;
	cfg_path = p_cfg_path;
	cfg_section = p_cfg_section;
	on_change = p_on_change;
	rows.clear();
}

void TuningSection::header(const String &p_text) {
	Row row;
	row.kind = ROW_HEADER;
	row.label = p_text;
	rows.push_back(row);
}

void TuningSection::slider(
		const String &p_label,
		const String &p_key,
		float *p_var,
		float p_min,
		float p_max,
		float p_step
) {
	Row row;
	row.kind = ROW_SLIDER;
	row.label = p_label;
	row.key = p_key;
	row.var = p_var;
	row.min = p_min;
	row.max = p_max;
	row.step = p_step;
	rows.push_back(row);
}

void TuningSection::slider(
		const String &p_label,
		const String &p_key,
		Object *p_object,
		float p_min,
		float p_max,
		float p_step
) {
	Row row;
	row.kind = ROW_SLIDER;
	row.label = p_label;
	row.key = p_key;
	row.object = p_object ? ObjectID(p_object->get_instance_id()) : ObjectID();
	row.min = p_min;
	row.max = p_max;
	row.step = p_step;
	rows.push_back(row);
}

void TuningSection::graph(
		const String &p_label,
		float p_min,
		float p_max
) {
	Row row;
	row.kind = ROW_GRAPH;
	row.label = p_label;
	row.min = p_min;
	row.max = p_max;
	rows.push_back(row);
}

// --- Lifetime ----------------------------------------------------------------

void TuningSection::attach() {
	if (id != 0) {
		return;
	}
	TuningPanel *panel = TuningPanel::ensure();
	if (panel) {
		id = panel->add_section(this);
		panel->set_section_shown(id, shown);
	}
}

void TuningSection::detach() {
	if (id == 0) {
		return;
	}
	TuningPanel *panel = TuningPanel::get_singleton();
	if (panel) {
		panel->remove_section(id); // frees the tab
	}
	id = 0;
	tab = ObjectID();
	for (Row &row : rows) {
		row.control = ObjectID();
	}
}

void TuningSection::set_shown(bool p_shown) {
	if (shown == p_shown) {
		return; // called every frame by some owners
	}
	shown = p_shown;
	TuningPanel *panel = TuningPanel::get_singleton();
	if (panel && id != 0) {
		panel->set_section_shown(id, shown);
	}
}

// --- Runtime -----------------------------------------------------------------

void TuningSection::push_graph(float p_value) {
	for (const Row &row : rows) {
		if (row.kind != ROW_GRAPH) {
			continue;
		}
		CUILineGraph *g = Object::cast_to<CUILineGraph>(ObjectDB::get_instance(row.control));
		if (g) {
			g->add_value(p_value);
		}
	}
}

void TuningSection::set_graph_range(
		float p_min,
		float p_max
) {
	for (Row &row : rows) {
		if (row.kind != ROW_GRAPH) {
			continue;
		}
		row.min = p_min;
		row.max = p_max;
		CUILineGraph *g = Object::cast_to<CUILineGraph>(ObjectDB::get_instance(row.control));
		if (g) {
			g->set_range(p_min, p_max);
		}
	}
}

void TuningSection::save() const {
	if (cfg_path.is_empty()) {
		return;
	}
	Ref<ConfigFile> cfg;
	cfg.instantiate();
	for (const Row &row : rows) {
		if (row.kind == ROW_SLIDER) {
			cfg->set_value(cfg_section, row.key, _get(row));
		}
	}
	cfg->save(cfg_path);
	UtilityFunctions::print("Tuning: ", title, " saved to ", cfg_path);
}

void TuningSection::load() {
	if (cfg_path.is_empty()) {
		return;
	}
	Ref<ConfigFile> cfg;
	cfg.instantiate();
	if (cfg->load(cfg_path) != OK) {
		return;
	}
	PackedStringArray sections = cfg->get_sections();
	for (const Row &row : rows) {
		if (row.kind != ROW_SLIDER) {
			continue;
		}
		// Prefer our section; fall back to any section holding the key (older files
		// split keys across sections). Missing keys keep the current value.
		String section = cfg_section;
		if (!cfg->has_section_key(section, row.key)) {
			section = String();
			for (const String &s : sections) {
				if (cfg->has_section_key(s, row.key)) {
					section = s;
					break;
				}
			}
		}
		if (!section.is_empty()) {
			_set(row, (float)cfg->get_value(section, row.key, _get(row)));
		}
	}
	_notify();
	_sync_sliders();
}

// --- Panel side --------------------------------------------------------------

void TuningSection::build(
		CUI *p_ui,
		Control *p_parent,
		int64_t p_id
) {
	tab = ObjectID(p_parent->get_instance_id());
	for (int i = 0; i < rows.size(); ++i) {
		Row &row = rows.write[i];
		if (row.kind == ROW_HEADER) {
			p_ui->add_header(p_parent, row.label);
			continue;
		}
		if (row.kind == ROW_GRAPH) {
			p_ui->add_label(p_parent, row.label);
			CUILineGraph *g = p_ui->add_graph(p_parent, vformat("tune_graph_%d_%d", p_id, i));
			g->set_range(row.min, row.max);
			g->set_max_points(120);
			row.control = ObjectID(g->get_instance_id());
			continue;
		}
		HBoxContainer *hbox = p_ui->add_hbox(p_parent);
		Label *label = p_ui->add_label(hbox, row.label);
		label->set_custom_minimum_size(Vector2(150, 0));
		// The value label is seeded from the initial value here: the panel may not be in
		// the tree yet, so later set_value() calls can't be relied on to refresh it.
		float value = _get(row);
		HSlider *s = p_ui->add_hslider(hbox, row.min, row.max, row.step, value, Callable());
		// Saved values may sit outside the slider's range: keep them rather than clamp.
		s->set_allow_greater(true);
		s->set_allow_lesser(true);
		s->set_value(value);
		s->connect("value_changed", callable_mp(TuningPanel::get_singleton(), &TuningPanel::_on_slider).bind(p_id, i));
		row.control = ObjectID(s->get_instance_id());
	}
}

void TuningSection::on_slider(
		int p_row,
		float p_value
) {
	if (p_row < 0 || p_row >= rows.size() || rows[p_row].kind != ROW_SLIDER) {
		return;
	}
	_set(rows[p_row], p_value);
	_notify();
}

// --- Internals ---------------------------------------------------------------

float TuningSection::_get(const Row &p_row) const {
	if (p_row.var) {
		return *p_row.var;
	}
	Object *obj = ObjectDB::get_instance(p_row.object);
	return obj ? (float)obj->get(p_row.key) : 0.0f;
}

void TuningSection::_set(
		const Row &p_row,
		float p_value
) {
	if (p_row.var) {
		*p_row.var = p_value;
		return;
	}
	Object *obj = ObjectDB::get_instance(p_row.object);
	if (obj) {
		obj->set(p_row.key, p_value);
	}
}

void TuningSection::_sync_sliders() {
	for (const Row &row : rows) {
		if (row.kind != ROW_SLIDER) {
			continue;
		}
		HSlider *s = Object::cast_to<HSlider>(ObjectDB::get_instance(row.control));
		if (s) {
			s->set_value(_get(row)); // same value back through on_slider: harmless
		}
	}
}

void TuningSection::_notify() {
	if (on_change) {
		on_change();
	}
}

} // namespace godot
