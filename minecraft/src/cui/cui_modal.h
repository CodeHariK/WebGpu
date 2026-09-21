#ifndef CUI_MODAL_H
#define CUI_MODAL_H

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/variant/callable.hpp>

#include "cui_overlay.h"

namespace godot {

/**
 * CUIModal — a reusable centred confirm / alert dialog on the CUIOverlay base.
 * `open_confirm()` shows title + message with Cancel / Confirm; `open_alert()`
 * shows a single OK. Emits `confirmed` / `cancelled`, runs an optional
 * `on_confirm` Callable, and Esc cancels. Domain-agnostic — any screen (game or
 * dev tool) can use it.
 */
class CUIModal : public CUIOverlay {
	GDCLASS(CUIModal,
			CUIOverlay)

private:
	Label *title_label = nullptr;
	Label *message_label = nullptr;
	HBoxContainer *button_row = nullptr;
	Callable pending_confirm;

	void _confirm();
	void _cancel();

protected:
	void _build_content() override;
	void _on_escape() override;
	static void _bind_methods();

public:
	CUIModal();
	~CUIModal();

	void open_confirm(const String &p_title, const String &p_message,
			const String &p_confirm_label, const String &p_cancel_label,
			const Callable &p_on_confirm);
	void open_alert(const String &p_title, const String &p_message,
			const String &p_ok_label);
};

} // namespace godot

#endif // CUI_MODAL_H
