#ifndef TUNING_SECTION_H
#define TUNING_SECTION_H

#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <functional>

namespace godot {

class CUI;
class Control;
class Object;

/**
 * One owner's page (tab) in the shared TuningPanel.
 *
 * An owner (character, vehicle, ...) keeps a TuningSection by value, describes
 * its tunables once by attaching its own variables, then calls attach() to get a
 * tab in the single on-screen panel:
 *
 * @code
 *   tuning.begin("Celeste", "user://celeste_settings.cfg", "Celeste", [this]() { _update_jump_math(); });
 *   tuning.header("Jump");
 *   tuning.slider("Jump Height", "jump_height", &jump_height, 0.5f, 12.0f, 0.1f);
 *   tuning.load();   // apply saved values (works with or without UI)
 *   tuning.attach(); // add the tab to the shared panel
 * @endcode
 *
 * Sliders write straight into the attached variable (or a bound Object property)
 * and then call the section's on_change hook so derived values can be rebuilt.
 * The tab is removed in detach() / the destructor, so owners never dangle.
 */
class TuningSection {
public:
	using ChangeFn = std::function<void()>;

	TuningSection() = default;
	~TuningSection();
	TuningSection(const TuningSection &) = delete;
	TuningSection &operator=(const TuningSection &) = delete;

	// --- Description (call before attach) ---------------------------------
	void begin(
			const String &p_title,
			const String &p_cfg_path,
			const String &p_cfg_section,
			ChangeFn p_on_change = ChangeFn()
	);
	void header(const String &p_text);
	/// Slider bound to a float the owner owns (must outlive the section).
	void slider(
			const String &p_label,
			const String &p_key,
			float *p_var,
			float p_min,
			float p_max,
			float p_step
	);
	/// Slider bound to a property of an Object/Resource (e.g. a config resource).
	void slider(
			const String &p_label,
			const String &p_key,
			Object *p_object,
			float p_min,
			float p_max,
			float p_step
	);
	/// A live line graph fed by push_graph().
	void graph(
			const String &p_label,
			float p_min,
			float p_max
	);

	// --- Lifetime ----------------------------------------------------------
	void attach(); ///< Build this tab in the shared panel (creates the panel on first use).
	void detach(); ///< Remove the tab; safe to call repeatedly or after the panel is gone.
	void set_shown(bool p_shown); ///< Hide the tab while the owner is inactive.

	// --- Runtime -----------------------------------------------------------
	void push_graph(float p_value);
	void set_graph_range(
			float p_min,
			float p_max
	);
	void save() const;
	void load();

	// --- Used by TuningPanel ----------------------------------------------
	void build(
			CUI *p_ui,
			Control *p_parent,
			int64_t p_id
	);
	void on_slider(
			int p_row,
			float p_value
	);
	const String &get_title() const { return title; }
	ObjectID get_tab() const { return tab; }

private:
	enum RowKind {
		ROW_HEADER,
		ROW_SLIDER,
		ROW_GRAPH,
	};
	struct Row {
		RowKind kind = ROW_HEADER;
		String label;
		String key;
		float *var = nullptr; ///< direct binding, or
		ObjectID object; ///< property binding (key is the property name)
		float min = 0.0f;
		float max = 1.0f;
		float step = 0.1f;
		ObjectID control; ///< the slider / graph once built
	};

	String title;
	String cfg_path;
	String cfg_section;
	ChangeFn on_change;
	Vector<Row> rows;
	int64_t id = 0; ///< registration id in the panel (0 = not attached)
	ObjectID tab;
	bool shown = true;

	float _get(const Row &p_row) const;
	void _set(
			const Row &p_row,
			float p_value
	);
	void _sync_sliders();
	void _notify();
};

} // namespace godot

#endif // TUNING_SECTION_H
