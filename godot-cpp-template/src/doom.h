#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>

using namespace godot;

enum DoomLightType {
	DOOM_LIGHT_GO,
	DOOM_LIGHT_STOP,
	DOOM_LIGHT_CAUTION,
};

class DoomLight : public Control {
	GDCLASS(DoomLight, Control);

	TextureRect *texture_rect;

	Ref<Texture2D> go_texture;
	Ref<Texture2D> stop_texture;
	Ref<Texture2D> caution_texture;

	DoomLightType doom_type;

protected:
	static void _bind_methods();

	void _notification(int p_what);

public:
	void set_go_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_go_texture() const;

	void set_stop_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_stop_texture() const;

	void set_caution_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_caution_texture() const;

	void set_doom_type(DoomLightType p_type);
	DoomLightType get_doom_type() const;

	DoomLight();
};

VARIANT_ENUM_CAST(DoomLightType);
