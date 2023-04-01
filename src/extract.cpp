#include "extract.hpp"

#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <iostream>

std::vector<std::string> extract_panorama_ids(rapidjson::Document& preview_document) {
	std::vector<std::string> ids;
	if(preview_document.IsArray() && preview_document.Size() == 11) {
		if(preview_document[0].IsArray() && preview_document[0].Size() > 0) {
			// std::cout << preview_document[0].Size() << " previews" << std::endl;
			for(auto& preview : preview_document[0].GetArray()) {
				if(preview.IsArray() && preview.Size() == 32) {
					if(preview[0].IsString()) {
						auto panorama_id
							= std::string(preview[0].GetString(), preview[0].GetStringLength());
						if(panorama_id.size() != 22) {
							// Not a street view
							continue;
						}
						ids.push_back(panorama_id);
					} else {
						std::cout << "Panorama id is not a string" << std::endl;
					}
				} else {
					std::cout << "Panorama is not well formed" << std::endl;
				}
			}
		} else {
			std::cout << "No street view panoramas here" << std::endl;
		}
	} else {
		std::cout << "Is not well formed array" << std::endl;
	}
	return ids;
}

Location extract_location(rapidjson::Document& photometa_document) {
	Location location;
	auto& outer_location = photometa_document[1][0][3];
	if(outer_location.IsArray() && outer_location.Size() > 2 && outer_location[2].IsArray()) {
		if(outer_location[2].Size() == 1) {
			// No address, just city
			location.city_and_state = std::string(outer_location[2][0].GetString());
		} else if(outer_location[2].Size() == 2) {
			// Address and city
			location.street         = std::string(outer_location[2][0].GetString());
			location.city_and_state = std::string(outer_location[2][1].GetString());
		}
	}

	return location;
}

std::vector<Panorama> extract_adjacent_panoramas(rapidjson::Document& photometa_document) {
	std::vector<Panorama> panoramas;
	auto& outer_adjacent = photometa_document[1][0][5];
	if(outer_adjacent.IsArray() && outer_adjacent.Size() == 1 && outer_adjacent[0].IsArray()
		&& outer_adjacent[0].Size() == 13 && outer_adjacent[0][3].IsArray()
		&& outer_adjacent[0][3].Size() == 1 && outer_adjacent[0][3][0].IsArray()) {
		for(auto& adjacent : outer_adjacent[0][3][0].GetArray()) {
			if(adjacent.IsArray() && adjacent.Size() == 3 && adjacent[0].IsArray()
				&& adjacent[0].Size() == 2) {
				Panorama panorama;
				auto adjacent_tile = adjacent[0][1].GetString();
				panorama.id        = std::string(adjacent_tile);
				if(adjacent[2].IsArray() && adjacent[2].Size() == 3 && adjacent[2][0].IsArray()
					&& adjacent[2][0].Size() == 4) {
					panorama.lat = adjacent[2][0][2].GetDouble();
					panorama.lng = adjacent[2][0][3].GetDouble();
				}
				panoramas.push_back(panorama);
			}
		}
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