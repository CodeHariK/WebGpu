#include "doom.h"

void DoomLight::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_go_texture", "texture"), &DoomLight::set_go_texture);
	ClassDB::bind_method(D_METHOD("get_go_texture"), &DoomLight::get_go_texture);

	ClassDB::bind_method(D_METHOD("set_stop_texture", "texture"), &DoomLight::set_stop_texture);
	ClassDB::bind_method(D_METHOD("get_stop_texture"), &DoomLight::get_stop_texture);

	ClassDB::bind_method(D_METHOD("set_caution_texture", "texture"), &DoomLight::set_caution_texture);
	ClassDB::bind_method(D_METHOD("get_caution_texture"), &DoomLight::get_caution_texture);

	ClassDB::bind_method(D_METHOD("set_doom_type", "type"), &DoomLight::set_doom_type);
	ClassDB::bind_method(D_METHOD("get_doom_type"), &DoomLight::get_doom_type);

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
			"set_doom_type",
			"get_doom_type");

	BIND_ENUM_CONSTANT(DoomLightType::DOOM_LIGHT_GO);
	BIND_ENUM_CONSTANT(DoomLightType::DOOM_LIGHT_STOP);
	BIND_ENUM_CONSTANT(DoomLightType::DOOM_LIGHT_CAUTION);
}

void DoomLight::_notification(int p_what) {
	switch (p_what) {
	NOTIFICATION_READY:
		set_doom_type(doom_type);
		break;
	}
}

void DoomLight::set_go_texture(const Ref<Texture2D> &p_texture) {
	go_texture = p_texture;
}
Ref<Texture2D> DoomLight::get_go_texture() const {
	return go_texture;
}

void DoomLight::set_stop_texture(const Ref<Texture2D> &p_texture) {
	stop_texture = p_texture;
}
Ref<Texture2D> DoomLight::get_stop_texture() const {
	return stop_texture;
}

void DoomLight::set_caution_texture(const Ref<Texture2D> &p_texture) {
	caution_texture = p_texture;
}
Ref<Texture2D> DoomLight::get_caution_texture() const {
	return caution_texture;
}

void DoomLight::set_doom_type(DoomLightType p_type) {
	doom_type = p_type;

	switch (p_type) {
		case DOOM_LIGHT_GO:
			texture_rect->set_texture(go_texture);
			break;
		case DOOM_LIGHT_STOP:
			texture_rect->set_texture(stop_texture);
			break;
		case DOOM_LIGHT_CAUTION:
			texture_rect->set_texture(caution_texture);
			break;
	}
}
DoomLightType DoomLight::get_doom_type() const {
	return doom_type;
}

DoomLight::DoomLight() {
	texture_rect = memnew(TextureRect);
	add_child(texture_rect);
	texture_rect->set_anchors_preset(PRESET_FULL_RECT);

	doom_type = DOOM_LIGHT_STOP;
}
