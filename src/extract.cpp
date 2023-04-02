#include "extract.hpp"

#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

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
	auto& id       = photometa_document[1][0][1][1];
	auto& lat_long = photometa_document[1][0][5][0][1][0];
	return Panorama {
		.id  = id.GetString(),
		.lat = lat_long[2].GetDouble(),
		.lng = lat_long[3].GetDouble(),
	};
}

Location extract_location(rapidjson::Document& photometa_document) {
	Location location;
	auto& outer_location = photometa_document[1][0][3][2];
	if(outer_location.Size() == 1) {
		// No address, just city
		location.city_and_state = std::string(outer_location[0].GetString());
	} else if(outer_location.Size() == 2) {
		// Address and city
		location.street         = std::string(outer_location[0].GetString());
		location.city_and_state = std::string(outer_location[1].GetString());
	}

	return location;
}

std::vector<Panorama> extract_adjacent_panoramas(rapidjson::Document& photometa_document) {
	std::vector<Panorama> panoramas;
	auto current_panorama = extract_info(photometa_document);
	auto& adjacent_list   = photometa_document[1][0][5][0][3][0];
	for(auto& adjacent : adjacent_list.GetArray()) {
		Panorama panorama = {
			.id  = adjacent[0][1].GetString(),
			.lat = adjacent[2][0][2].GetDouble(),
			.lng = adjacent[2][0][3].GetDouble(),
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