#pragma once

#include <rapidjson/document.h>

#include <string>
#include <utility>
#include <vector>

struct Location {
	std::string street;
	std::string city_and_state;
};

struct Panorama {
	double lat;
	double lng;
	double yaw;
	double pitch;
	double roll;
	std::string id;
};

std::vector<std::string> extract_panorama_ids(rapidjson::Document& preview_document);
Panorama extract_info(rapidjson::Document& photometa_document);
Location extract_location(rapidjson::Document& photometa_document);
std::vector<Panorama> extract_adjacent_panoramas(rapidjson::Document& photometa_document);
std::pair<double, double> extract_tiles_dimensions(
	rapidjson::Document& photometa_document, int streetview_zoom);