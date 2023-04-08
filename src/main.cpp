#define SK_ENCODE_PNG

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <GLFW/glfw3.h>
#include <codec/SkCodec.h>
#include <codec/SkEncodedImageFormat.h>
#include <core/SkBitmap.h>
#include <core/SkCanvas.h>
#include <core/SkColor.h>
#include <core/SkFont.h>
#include <core/SkGraphics.h>
#include <core/SkImage.h>
#include <core/SkImageEncoder.h>
#include <core/SkStream.h>
#include <core/SkSurface.h>
#include <core/SkTextBlob.h>
#include <core/SkTypeface.h>
#include <curl/curl.h>
#include <encode/SkPngEncoder.h>
#include <fmt/args.h>
#include <fmt/format.h>
#include <gpu/GrDirectContext.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

#include "download.hpp"
#include "extract.hpp"
#include "headers.hpp"
#include "interface.hpp"
#include "parse.hpp"

int main(int argc, char** argv) {
	CLI::App app { "Street View custom client in C++" };
	app.require_subcommand(1, 1);

	auto& download_sub = *app.add_subcommand("download", "Download panoramas");
	double lat;
	download_sub.add_option("--lat", lat, "Latitude")->required();
	double lng;
	download_sub.add_option("--long", lng, "Longitude")->required();
	int range = 10000;
	download_sub.add_option("-r,--range", range, "Range from location");
	int month_start = 1;
	download_sub.add_option("--month-start", month_start, "Starting month");
	int month_end = 12;
	download_sub.add_option("--month-end", month_end, "Ending month (inclusive)");
	int year_start = 0;
	download_sub.add_option("--year-start", year_start, "Starting year");
	int year_end = 10000;
	download_sub.add_option("--year-end", year_end, "Ending year (inclusive)");
	std::string filepath_format = "tiles/{id}";
	download_sub.add_option(
		"--path-format", filepath_format, "Path format, not including the extension");
	int num_panoramas = 100;
	download_sub.add_option(
		"-n,--num-panoramas", num_panoramas, "Number of panoramas to attempt to download");
	int streetview_zoom = 2;
	download_sub.add_option("-z,--zoom", streetview_zoom,
		"How much to zoom in street view images, higher numbers "
		"increase resolution");
	bool include_json_info = false;
	download_sub.add_flag(
		"-j,--json", include_json_info, "Include JSON info alongside, used to render directories");

	auto& download_recursive_sub
		= *download_sub.add_subcommand("recursive", "Recursively download");
	int num_recursive_attempts = 10;
	download_recursive_sub.add_option("-a,--num-attempts", num_recursive_attempts,
		"Number of recursive attempts to download more images");
	double recursive_radius = 10;
	download_recursive_sub.add_option(
		"-r,--radius", recursive_radius, "Number of recursive attempts to download more images");

	auto& render_sub = *app.add_subcommand("render", "Render panoramas");
	std::string initial_id;
	render_sub.add_option("-i,--id", initial_id, "Initial panorama ID")->required();

	CLI11_PARSE(app, argc, argv);

	curl_global_init(CURL_GLOBAL_ALL);
	if(download_sub) {
		auto curl_handle = curl_easy_init();

		if(download_recursive_sub) {
			auto start = std::chrono::high_resolution_clock::now();

			auto client_id = download_client_id(curl_handle);

			// Location of all downloaded panoramas
			std::unordered_set<std::string> already_downloaded;
			std::unordered_set<std::string> already_have;

			// Download initial starting point
			auto initial_preview_document
				= download_preview_document(curl_handle, client_id, num_panoramas, lat, lng, range);
			auto panorama_ids = extract_panorama_ids(initial_preview_document);
			auto sorted_infos = get_infos(curl_handle, client_id, panorama_ids);
			sort_by_distance(lat, lng, sorted_infos);
			for(auto& info : sorted_infos) {
				already_have.emplace(info.id);
			}

			for(int i = 0; i < num_recursive_attempts; i++) {
				for(auto& panorama : sorted_infos) {
					// Choose the first panorama, sorted in ascending distance, that we have not
					// downloaded yet
					if(!already_downloaded.count(panorama.id)) {
						already_downloaded.emplace(panorama.id);

						// Download this one and get its adjacent
						auto photometa_document
							= download_photometa(curl_handle, client_id, panorama.id);
						if(!valid_photometa(photometa_document)) {
							continue;
						}
						auto adjacent = extract_adjacent_panoramas(photometa_document);

						// Insert only the ones we don't already have
						for(auto& info : adjacent) {
							if(!already_have.count(info.id)) {
								sorted_infos.push_back(info);
								already_have.emplace(info.id);
							}
						}

						// Resort the array and go again
						sort_by_distance(lat, lng, sorted_infos);

						// Print the number of panoramas we have total and also the number within
						// the radius we set
						fmt::print("Number so far: {} Number within radius: {}\n",
							sorted_infos.size(),
							num_within_distance(lat, lng, recursive_radius, sorted_infos));

						// Break out of this loop for another attempt
						break;
					}
				}
			}

			auto stop = std::chrono::high_resolution_clock::now();
			fmt::print("Downloading panorama list took {}ms\n",
				std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());

			// Download all the panoramas within the distance
			for(auto& panorama : sorted_infos) {
				if(center_distance(lat, lng, panorama) <= recursive_radius) {
					auto start = std::chrono::high_resolution_clock::now();

					// Redownload the photometa, most images were adjacent and thus didn't have a
					// photometa to begin with
					auto photometa_document
						= download_photometa(curl_handle, client_id, panorama.id);

					// Get info
					auto panorama_info = extract_info(photometa_document);

					// Get panorama
					auto tile_surface = download_panorama(
						curl_handle, panorama.id, streetview_zoom, photometa_document);

					// Location
					auto location = extract_location(photometa_document);

					std::string filename = fmt::format(fmt::runtime(filepath_format),
						fmt::arg("id", panorama_info.id), fmt::arg("year", panorama_info.year),
						fmt::arg("month", panorama_info.month), fmt::arg("lat", panorama_info.lat),
						fmt::arg("long", panorama_info.lng));
					std::filesystem::create_directories(
						std::filesystem::path(filename).parent_path());

					auto tile_data = tile_surface->encodeToData(SkEncodedImageFormat::kPNG, 95);
					std::ofstream outfile(filename + ".png", std::ios::out | std::ios::binary);
					outfile.write((const char*)tile_data->bytes(), tile_data->size());
					outfile.close();

					if(include_json_info) {
						// Include JSON info alongside
						rapidjson::Document infoJson(rapidjson::kObjectType);
						infoJson.AddMember("id", panorama_info.id, infoJson.GetAllocator());
						infoJson.AddMember("year", panorama_info.year, infoJson.GetAllocator());
						infoJson.AddMember("month", panorama_info.month, infoJson.GetAllocator());
						infoJson.AddMember(
							"location", location.city_and_state, infoJson.GetAllocator());
						infoJson.AddMember("lat", panorama_info.lat, infoJson.GetAllocator());
						infoJson.AddMember("long", panorama_info.lng, infoJson.GetAllocator());

						rapidjson::StringBuffer infoSb;
						rapidjson::PrettyWriter<rapidjson::StringBuffer> infoWriter(infoSb);
						infoWriter.SetIndent('\t', 1);
						infoJson.Accept(infoWriter);

						// Write to filesystem at the same location the panorama is
						std::ofstream infoFile(filename + ".json", std::ios::out);
						infoFile.write(infoSb.GetString(), infoSb.GetLength());
						infoFile.close();
					}

					auto stop = std::chrono::high_resolution_clock::now();
					fmt::print("Downloading {} took {}ms\n", panorama.id,
						std::chrono::duration_cast<std::chrono::milliseconds>(stop - start)
							.count());
				}
			}
		} else {
			std::chrono::time_point<std::chrono::steady_clock> start;
			std::chrono::time_point<std::chrono::steady_clock> stop;

			start = std::chrono::high_resolution_clock::now();

			auto curl_handle = curl_easy_init();

			auto client_id = download_client_id(curl_handle);

			auto preview_document
				= download_preview_document(curl_handle, client_id, num_panoramas, lat, lng, range);

			stop = std::chrono::high_resolution_clock::now();
			fmt::print("Setup took {}ms\n",
				std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());

			for(auto& panorama_id : extract_panorama_ids(preview_document)) {
				start = std::chrono::high_resolution_clock::now();

				// Get photometa
				auto photometa_document = download_photometa(curl_handle, client_id, panorama_id);

				// Get info
				auto panorama_info = extract_info(photometa_document);

				// Check if it is within the range
				if(panorama_info.month >= month_start && panorama_info.month <= month_end
					&& panorama_info.year >= year_start && panorama_info.year <= year_end) {
					// Get panorama
					auto tile_surface = download_panorama(
						curl_handle, panorama_id, streetview_zoom, photometa_document);

					// Location
					auto location = extract_location(photometa_document);

					std::string filename = fmt::format(fmt::runtime(filepath_format + ".png"),
						fmt::arg("id", panorama_id), fmt::arg("year", panorama_info.year),
						fmt::arg("month", panorama_info.month),
						fmt::arg("location", location.city_and_state),
						fmt::arg("lat", panorama_info.lat), fmt::arg("long", panorama_info.lng));
					std::filesystem::create_directories(
						std::filesystem::path(filename).parent_path());

					auto tile_data = tile_surface->encodeToData(SkEncodedImageFormat::kPNG, 95);
					std::ofstream outfile(filename, std::ios::out | std::ios::binary);
					outfile.write((const char*)tile_data->bytes(), tile_data->size());
					outfile.close();

					stop = std::chrono::high_resolution_clock::now();
					fmt::print("Downloading {} took {}ms\n", panorama_id,
						std::chrono::duration_cast<std::chrono::milliseconds>(stop - start)
							.count());
				}
			}
		}

		curl_easy_cleanup(curl_handle);
		curl_global_cleanup();
	} else if(render_sub) {
		auto curl_handle = curl_easy_init();
		InterfaceWindow window(initial_id, curl_handle);
		window.PrepareWindow();
		while(!window.ShouldClose()) {
			window.DrawFrame();
		}
		curl_easy_cleanup(curl_handle);
	}

	return 0;
}