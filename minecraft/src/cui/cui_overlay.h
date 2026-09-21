#ifndef CUI_OVERLAY_H
#define CUI_OVERLAY_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/color.hpp>

namespace godot {

class CUI;

/**
 * CUIOverlay — reusable base for fading, full-screen overlays (menus, dialogs,
 * title screens). Owns the shared machinery every such screen used to duplicate:
 *   - an OPENING → OPEN → CLOSING → CLOSED state machine
 *   - open()/close()/toggle() with a CUITween fade that kills any prior fade
 *   - a dim backdrop (optional) + a full-rect CenterContainer (`center`) that
 *     subclasses build their content into
 *   - fit-to-parent on open + Esc forwarding via `_on_escape()`
 *
 * A `CUI` builder is injected (`set_builder`) before the node enters the tree;
 * subclasses override `_build_content()` to add their widgets to `center`, and
 * (optionally) `_on_escape()`. No dependency on any game system.
 */
class CUIOverlay : public Control {
	GDCLASS(CUIOverlay,
			Control)

public:
	enum State {
		STATE_CLOSED,
		STATE_OPENING,
		STATE_OPEN,
		STATE_CLOSING,
	};

protected:
	CUI *builder = nullptr;
	Control *backdrop = nullptr;
	Control *center = nullptr; // CenterContainer content lives in
	State state = STATE_CLOSED;
	Ref<Tween> anim;

	// Subclass-tunable (set in the subclass constructor).
	bool has_backdrop = true;
	Color backdrop_color = Color(0.0, 0.0, 0.0, 0.5);
	double fade_in_duration = 0.18;
	double fade_out_duration = 0.18;

	virtual void _build_content() {} // subclass adds widgets to `center`
	virtual void _on_escape() {} // subclass reacts to Esc (default: nothing)

	void _fit_parent();
	void _on_finish_open();
	void _on_finish_close();

	static void _bind_methods();

public:
	CUIOverlay();
	~CUIOverlay();

	void set_builder(CUI *p_builder) { builder = p_builder; }

	void _ready() override;
	void _input(const Ref<InputEvent> &p_event) override;

	void open();
	void close();
	void toggle();
	bool is_open() const { return state == STATE_OPEN || state == STATE_OPENING; }
	int get_state() const { return (int)state; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::CUIOverlay::State);

#endif // CUI_OVERLAY_H
