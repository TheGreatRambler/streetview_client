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
	int month = 0;
	int year  = 0;
	std::string id;
};

std::vector<std::string> extract_panorama_ids(rapidjson::Document& preview_document);
Panorama extract_info(rapidjson::Document& photometa_document);
Location extract_location(rapidjson::Document& photometa_document);
std::vector<Panorama> extract_adjacent_panoramas(rapidjson::Document& photometa_document);
std::pair<double, double> extract_tiles_dimensions(
	rapidjson::Document& photometa_document, int streetview_zoom);
bool valid_photometa(rapidjson::Document& photometa_document);
double center_distance(double lat, double lng, Panorama& panorama);
int num_within_distance(double lat, double lng, double radius, std::vector<Panorama>& panoramas);
void sort_by_distance(double lat, double lng, std::vector<Panorama>& panoramas);