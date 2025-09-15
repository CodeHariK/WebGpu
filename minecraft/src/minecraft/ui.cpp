#include "minecraft.h"

#include "godot_cpp/classes/button.hpp"
#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/progress_bar.hpp"
#include "godot_cpp/classes/slider.hpp"

#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/classes/tween.hpp"
#include "godot_cpp/variant/string.hpp"
#include <cstdlib>
#include <godot_cpp/classes/property_tweener.hpp>

void MinecraftNode::setup_ui() {
	tween = create_tween();

	String ui_root = "/root/World/UiMinecraft/";
	ui.health_bar = Object::cast_to<ProgressBar>(get_node_or_null(ui_root + "HealthBar"));
	ui.header_button = Object::cast_to<Button>(get_node_or_null(ui_root + "TerrainProperties/TerrainHeaderButton"));
	ui.terrain_slider = Object::cast_to<Slider>(get_node_or_null(ui_root + "TerrainProperties/TerrainContent/TerrainSizeSlider"));
	ui.content = Object::cast_to<Control>(get_node_or_null(ui_root + "TerrainProperties/TerrainContent"));
	if (ui.is_valid()) {
		ui.header_button->connect("pressed", Callable(this, "ui_on_header_button_pressed"));
		ui.terrain_slider->connect("value_changed", Callable(this, "ui_on_terrain_slider_change"));

		// Connect mouse enter/exit
		ui.header_button->connect("mouse_entered", Callable(this, "ui_on_header_mouse_entered"));
		ui.header_button->connect("mouse_exited", Callable(this, "ui_on_header_mouse_exited"));
	}
}

void MinecraftNode::ui_on_header_button_pressed() {
	bool new_visible = !ui.content->is_visible();
	ui.content->set_visible(new_visible);
	ui.header_button->set_text(new_visible ? "Hide" : "Show");

	ui.health_bar->set_value(rand() % 101);
}

void MinecraftNode::ui_on_terrain_slider_change(double value) {
	terrain_len_x = (int)value;
	terrain_len_z = (int)value;
	terrain_dirty = true;
}

void MinecraftNode::ui_on_header_mouse_entered() {
	if (!tween->is_valid())
		tween->kill();
	tween = create_tween();

	tween->tween_property(ui.header_button, "scale", Vector2(1.1, 1.1), 0.2)
			->set_trans(Tween::TransitionType::TRANS_SINE)
			->set_ease(Tween::EaseType::EASE_OUT);

	tween->tween_property(ui.header_button, "modulate", Color(1.2, 0, 1.2), 0.2)
			->set_trans(Tween::TransitionType::TRANS_SINE)
			->set_ease(Tween::EaseType::EASE_OUT);
}

void MinecraftNode::ui_on_header_mouse_exited() {
	if (!tween->is_valid())
		tween->kill();
	tween = create_tween();

	tween->tween_property(ui.header_button, "scale", Vector2(1.0, 1.0), 0.2)
			->set_trans(Tween::TransitionType::TRANS_SINE)
			->set_ease(Tween::EaseType::EASE_IN);

	tween->tween_property(ui.header_button, "modulate", Color(1.0, 1.0, 1.0), 0.2)
			->set_trans(Tween::TransitionType::TRANS_SINE)
			->set_ease(Tween::EaseType::EASE_IN);
}
