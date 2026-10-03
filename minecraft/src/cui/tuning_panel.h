#ifndef TUNING_PANEL_H
#define TUNING_PANEL_H

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>

namespace godot {

class Button;
class CUI;
class Panel;
class TabContainer;
class TuningSection;

/**
 * The one live-tuning panel shared by every system (characters, vehicles, ...).
 *
 * A single "Tuning" button opens a tabbed panel; each TuningSection that calls
 * attach() becomes one tab. Save / Load act on every attached section, each
 * writing its own config file. The panel is created lazily under the scene root
 * on first use and outlives scene changes; sections remove their tabs on detach.
 */
class TuningPanel : public CanvasLayer {
	GDCLASS(TuningPanel,
			CanvasLayer)

private:
	static TuningPanel *singleton;
	static int64_t next_id; ///< global so ids are never reused across panel instances

	CUI *ui = nullptr;
	Panel *panel = nullptr;
	TabContainer *tabs = nullptr;
	HashMap<int64_t, TuningSection *> sections;
	HashMap<int64_t, uint64_t> tab_ids; ///< section id -> tab ScrollContainer instance id

	void _build();
	int _tab_index(int64_t p_id) const;

protected:
	static void _bind_methods();

public:
	TuningPanel();
	~TuningPanel();

	static TuningPanel *get_singleton() { return singleton; }
	/// The panel, creating it under the scene root if needed (nullptr if no tree yet).
	static TuningPanel *ensure();

	int64_t add_section(TuningSection *p_section);
	void remove_section(int64_t p_id);
	void set_section_shown(
			int64_t p_id,
			bool p_shown
	);

	void toggle();
	bool is_open() const;
	void save_all();
	void load_all();

	void _on_slider(
			double p_value,
			int64_t p_id,
			int p_row
	);
};

} // namespace godot

#endif // TUNING_PANEL_H
