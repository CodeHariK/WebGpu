#pragma once

#include "json.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace world_blender {
using nlohmann::json;

inline json get_untyped(const json &j, const char *property) {
	if (j.find(property) != j.end()) {
		return j.at(property).get<json>();
	}
	return json();
}

inline json get_untyped(const json &j, std::string property) {
	return get_untyped(j, property.data());
}

struct LibraryItem {
	std::string obj;
	int16_t hashNx;
	int16_t hashPx;
	int16_t hashNy;
	int16_t hashPy;
	int16_t hashPz;
	int16_t hashNz;
};

struct Data {
	std::vector<LibraryItem> items;
	std::vector<std::string> voxels;
};

inline void from_json(const json &j, LibraryItem &x) {
	x.obj = j.at("obj").get<std::string>();

	x.hashNx = j.at("hash_NX").get<int16_t>();
	x.hashPx = j.at("hash_PX").get<int16_t>();

	x.hashNy = j.at("hash_NZ").get<int16_t>();
	x.hashPy = j.at("hash_PZ").get<int16_t>();

	x.hashNz = j.at("hash_PY").get<int16_t>();
	x.hashPz = j.at("hash_NY").get<int16_t>();
}

inline void to_json(json &j, const LibraryItem &x) {
	j = json::object();
	j["obj"] = x.obj;

	j["hash_NX"] = x.hashNx;
	j["hash_PX"] = x.hashPx;

	j["hash_NZ"] = x.hashNy;
	j["hash_PZ"] = x.hashPy;

	j["hash_PY"] = x.hashNz;
	j["hash_NY"] = x.hashPz;
}

inline void from_json(const json &j, Data &x) {
	x.items = j.at("items").get<std::vector<LibraryItem>>();
	x.voxels = j.at("voxels").get<std::vector<std::string>>();
}

inline void to_json(json &j, const Data &x) {
	j = json::object();
	j["items"] = x.items;
	j["voxels"] = x.voxels;
}
} //namespace world_blender
