#ifndef FOLIO_MENU_H
#define FOLIO_MENU_H

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>

namespace godot {

/**
 * Folio port — FolioMenu  (folio `Game/Menu.js`, re-imagined as a Godot overlay)
 * -----------------------------------------------
 * A centred panel overlay that fades open/closed on the FolioUI open/close state
 * machine (folio Menu.OPENING/OPEN/CLOSING/CLOSED). It lives under FolioUI's root
 * Control and is the first real screen built on the UI foundation. Toggled by Esc
 * or the HUD menu button.
 *
 * Rows demonstrate the framework driving real systems: a Sound row (FolioAudio
 * mute) and a Quality row (FolioQuality.change_level, which grass/rendering already
 * react to). A Resume row closes it. Later screens (Modals, Title, Map) follow the
 * same open/close pattern.
 */
class FolioMenu : public Control {
	GDCLASS(FolioMenu,
			Control)

public:
	enum State {
		STATE_CLOSED,
		STATE_OPENING,
		STATE_OPEN,
		STATE_CLOSING,
	};

private:
	Control *backdrop = nullptr; // dim + click-catch behind the panel
	Button *sound_button = nullptr;
	Button *quality_button = nullptr;
	State state = STATE_CLOSED;

	void _build();
	void _refresh_sound(bool p_muted);
	void _refresh_quality();
	void _on_sound_pressed();
	void _on_quality_pressed();
	void _on_mute_changed(bool p_active);
	void _on_finish_open();
	void _on_finish_close();

protected:
	static void _bind_methods();

public:
	FolioMenu();
	~FolioMenu();

	void _ready() override;
	void _input(const Ref<InputEvent> &p_event) override;

	void open();
	void close();
	void toggle();
	bool is_open() const { return state == STATE_OPEN || state == STATE_OPENING; }
	int get_state() const { return (int)state; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::FolioMenu::State);

#endif // FOLIO_MENU_H
