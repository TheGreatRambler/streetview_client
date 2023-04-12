#include "extract.hpp"

#define _USE_MATH_DEFINES
#define DEG_RAD 0.0174533

#include <fmt/format.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cmath>
#include <iostream>

std::vector<std::string> extract_panorama_ids(rapidjson::Document& preview_document) {
	std::vector<std::string> ids;
	// std::cout << preview_document[0].Size() << " previews" << std::endl;
	for(auto& preview : preview_document[0].GetArray()) {
		auto panorama_id = std::string(preview[0].GetString(), preview[0].GetStringLength());
		if(panorama_id.size() != 22) {
			// Not a street view
			continue;
		}
		ids.push_back(panorama_id);
	}
	return ids;
}

Panorama extract_info(rapidjson::Document& photometa_document) {
	auto& id             = photometa_document[1][0][1][1];
	auto& lat_long       = photometa_document[1][0][5][0][1][0];
	auto& yaw_pitch_roll = photometa_document[1][0][5][0][1][2];
	auto& year_month     = photometa_document[1][0][6][7];
	return Panorama {
		.id    = id.GetString(),
		.lat   = lat_long[2].GetDouble(),
		.lng   = lat_long[3].GetDouble(),
		.yaw   = yaw_pitch_roll[0].GetDouble() * DEG_RAD,
		.pitch = yaw_pitch_roll[1].GetDouble() * DEG_RAD,
		.roll  = yaw_pitch_roll[2].GetDouble() * DEG_RAD,
		.month = year_month[1].GetInt(),
		.year  = year_month[0].GetInt(),
	};
}

Location extract_location(rapidjson::Document& photometa_document) {
	Location location;
	// Can segfault
	if(photometa_document[1][0][3].Size() > 2) {
		auto& outer_location = photometa_document[1][0][3][2];
		if(outer_location.Size() == 1) {
			// No address, just city
			location.city_and_state = std::string(outer_location[0].GetString());
		} else if(outer_location.Size() == 2) {
			// Address and city
			location.street         = std::string(outer_location[0].GetString());
			location.city_and_state = std::string(outer_location[1].GetString());
		}
	}
	return location;
}

std::vector<Panorama> extract_adjacent_panoramas(rapidjson::Document& photometa_document) {
	std::vector<Panorama> panoramas;
	auto current_panorama = extract_info(photometa_document);
	auto& adjacent_list   = photometa_document[1][0][5][0][3][0];
	for(auto& adjacent : adjacent_list.GetArray()) {
		Panorama panorama = {
			.id    = adjacent[0][1].GetString(),
			.lat   = adjacent[2][0][2].GetDouble(),
			.lng   = adjacent[2][0][3].GetDouble(),
			.yaw   = adjacent[2][2][0].GetDouble() * DEG_RAD,
			.pitch = adjacent[2][2][1].GetDouble() * DEG_RAD,
			.roll  = adjacent[2][2][2].GetDouble() * DEG_RAD,
		};
		if(panorama.id == current_panorama.id)
			continue;
		panoramas.push_back(panorama);
	}
	return panoramas;
}

std::pair<double, double> extract_tiles_dimensions(
	rapidjson::Document& photometa_document, int streetview_zoom) {
	auto& tiles_dimensions = photometa_document[1][0][2][2];
	double tiles_width     = tiles_dimensions[1].GetInt() / 512 / pow(2, 5 - streetview_zoom);
	double tiles_height    = tiles_dimensions[0].GetInt() / 512 / pow(2, 5 - streetview_zoom);
	return std::make_pair(tiles_width, tiles_height);
}

bool valid_photometa(rapidjson::Document& photometa_document) {
	// For starters
	return photometa_document.Size() > 0;
}

double center_distance(double lat, double lng, Panorama& panorama) {
	return std::sqrt(std::pow(panorama.lat - lat, 2) + std::pow(panorama.lng - lng, 2));
}

int num_within_distance_and_date(double lat, double lng, double radius, int year_start,
	int year_end, int month_start, int month_end, std::vector<Panorama>& panoramas) {
	int num = 0;
	for(auto& panorama : panoramas) {
		if(is_within_distance_and_date(
			   lat, lng, radius, year_start, year_end, month_start, month_end, panorama)) {
			num++;
		}
	}
	return num;
}

bool is_within_distance_and_date(double lat, double lng, double radius, int year_start,
	int year_end, int month_start, int month_end, Panorama& panorama) {
	return center_distance(lat, lng, panorama) <= radius && panorama.month >= month_start
		   && panorama.month <= month_end && panorama.year >= year_start
		   && panorama.year <= year_end;
}

bool is_within_date(
	int year_start, int year_end, int month_start, int month_end, Panorama& panorama) {
	return panorama.month >= month_start && panorama.month <= month_end
		   && panorama.year >= year_start && panorama.year <= year_end;
	;
}

void sort_by_distance(double lat, double lng, std::vector<Panorama>& panoramas) {
	std::sort(panoramas.begin(), panoramas.end(), [&](Panorama& a, Panorama& b) {
		return center_distance(lat, lng, a) < center_distance(lat, lng, b);
	});
}

bool is_date_specified(int year_start, int year_end, int month_start, int month_end) {
	return !(year_start == -1 && year_end == 10000 && month_start == -1 && month_end == 10000);
}