#ifndef OC_ORDER_UI_H
#define OC_ORDER_UI_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/label.hpp>

namespace godot {

class OCOrderUI : public Control {
	GDCLASS(OCOrderUI, Control)

private:
	VBoxContainer *order_list = nullptr;
	Label *score_label = nullptr;
	
	void rebuild_ui();

protected:
	static void _bind_methods();

public:
	OCOrderUI();
	~OCOrderUI();

	void _ready() override;
	void _process(double delta) override;
	void _exit_tree() override;
};

} // namespace godot

#endif // OC_ORDER_UI_H
