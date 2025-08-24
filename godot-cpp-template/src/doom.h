#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>

using namespace godot;

enum DoomTexEnum {
	DOOM_TEX_GO,
	DOOM_TEX_STOP,
	DOOM_TEX_CAUTION,
};

class Doom : public Control {
	GDCLASS(Doom, Control);

	TextureRect *texture_rect;

	Ref<Texture2D> go_texture;
	Ref<Texture2D> stop_texture;
	Ref<Texture2D> caution_texture;

	DoomTexEnum doom_type;

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

	void set_doom_tex(DoomTexEnum p_type);
	DoomTexEnum get_doom_tex() const;

	Doom();
};

VARIANT_ENUM_CAST(DoomTexEnum);
