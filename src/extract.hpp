#pragma once

#include <rapidjson/document.h>

#include <string>
#include <vector>

struct Location {
	std::string street;
	std::string city_and_state;
};

struct Panorama {
	double lat;
	double lng;
	std::string id;
};

Location extract_location(rapidjson::Document& photometa_document);
std::vector<Panorama> extract_adjacent_panoramas(rapidjson::Document& photometa_document);