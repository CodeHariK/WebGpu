#pragma once

#include "json.hpp"

namespace Celebi {
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
	int64_t hashNx;
	int64_t hashPx;
	int64_t hashNy;
	int64_t hashPy;
	int64_t hashPz;
	int64_t hashNz;
};

struct Data {
	std::vector<LibraryItem> items;
	std::vector<std::string> voxels;
};

inline void from_json(const json &j, LibraryItem &x) {
	x.obj = j.at("obj").get<std::string>();
	x.hashNx = j.at("hash_NX").get<int64_t>();
	x.hashPx = j.at("hash_PX").get<int64_t>();
	x.hashNy = j.at("hash_NY").get<int64_t>();
	x.hashPy = j.at("hash_PY").get<int64_t>();
	x.hashPz = j.at("hash_PZ").get<int64_t>();
	x.hashNz = j.at("hash_NZ").get<int64_t>();
}

inline void to_json(json &j, const LibraryItem &x) {
	j = json::object();
	j["obj"] = x.obj;
	j["hash_NX"] = x.hashNx;
	j["hash_PX"] = x.hashPx;
	j["hash_NY"] = x.hashNy;
	j["hash_PY"] = x.hashPy;
	j["hash_PZ"] = x.hashPz;
	j["hash_NZ"] = x.hashNz;
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
} //namespace Celebi
