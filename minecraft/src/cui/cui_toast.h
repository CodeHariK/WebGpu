#ifndef CUI_TOAST_H
#define CUI_TOAST_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class CUI;

/**
 * CUIToast — a reusable top-centre transient toast stack. `notify(text)` drops a
 * themed pill that fades in, holds, then fades out and frees itself (via
 * CUITween). Builder injected with `set_builder`. Domain-agnostic.
 */
class CUIToast : public Control {
	GDCLASS(CUIToast,
			Control)

private:
	CUI *builder = nullptr;
	VBoxContainer *stack = nullptr;
	double default_duration = 2.5;

protected:
	static void _bind_methods();

public:
	CUIToast();
	~CUIToast();

	void set_builder(CUI *p_builder) { builder = p_builder; }

	void _ready() override;
	void notify(const String &p_text, double p_duration = -1.0);
};

} // namespace godot

#endif // CUI_TOAST_H
