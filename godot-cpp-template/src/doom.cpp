#include "doom.h"

void Doom::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_go_texture", "texture"), &Doom::set_go_texture);
	ClassDB::bind_method(D_METHOD("get_go_texture"), &Doom::get_go_texture);

	ClassDB::bind_method(D_METHOD("set_stop_texture", "texture"), &Doom::set_stop_texture);
	ClassDB::bind_method(D_METHOD("get_stop_texture"), &Doom::get_stop_texture);

	ClassDB::bind_method(D_METHOD("set_caution_texture", "texture"), &Doom::set_caution_texture);
	ClassDB::bind_method(D_METHOD("get_caution_texture"), &Doom::get_caution_texture);

	ClassDB::bind_method(D_METHOD("set_doom_tex", "type"), &Doom::set_doom_tex);
	ClassDB::bind_method(D_METHOD("get_doom_tex"), &Doom::get_doom_tex);

	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "go_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"),
			"set_go_texture",
			"get_go_texture");
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "stop_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"),
			"set_stop_texture",
			"get_stop_texture");
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "caution_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"),
			"set_caution_texture",
			"get_caution_texture");

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "doom_type", PROPERTY_HINT_ENUM, "Go,Stop,Caution"),
			"set_doom_tex",
			"get_doom_tex");

	BIND_ENUM_CONSTANT(DoomTexEnum::DOOM_TEX_GO);
	BIND_ENUM_CONSTANT(DoomTexEnum::DOOM_TEX_STOP);
	BIND_ENUM_CONSTANT(DoomTexEnum::DOOM_TEX_CAUTION);
}

void Doom::_notification(int p_what) {
	switch (p_what) {
	NOTIFICATION_READY:
		set_doom_tex(doom_type);
		break;
	}
}

void Doom::set_go_texture(const Ref<Texture2D> &p_texture) {
	go_texture = p_texture;
}
Ref<Texture2D> Doom::get_go_texture() const {
	return go_texture;
}

void Doom::set_stop_texture(const Ref<Texture2D> &p_texture) {
	stop_texture = p_texture;
}
Ref<Texture2D> Doom::get_stop_texture() const {
	return stop_texture;
}

void Doom::set_caution_texture(const Ref<Texture2D> &p_texture) {
	caution_texture = p_texture;
}
Ref<Texture2D> Doom::get_caution_texture() const {
	return caution_texture;
}

void Doom::set_doom_tex(DoomTexEnum p_type) {
	doom_type = p_type;

	switch (p_type) {
		case DOOM_TEX_GO:
			texture_rect->set_texture(go_texture);
			break;
		case DOOM_TEX_STOP:
			texture_rect->set_texture(stop_texture);
			break;
		case DOOM_TEX_CAUTION:
			texture_rect->set_texture(caution_texture);
			break;
	}
}
DoomTexEnum Doom::get_doom_tex() const {
	return doom_type;
}

Doom::Doom() {
	texture_rect = memnew(TextureRect);
	add_child(texture_rect);
	texture_rect->set_anchors_preset(PRESET_FULL_RECT);

	doom_type = DOOM_TEX_STOP;
}
