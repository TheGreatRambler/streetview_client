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
	int num_previews = 100;
	download_sub.add_option("-n,--num-previews", num_previews, "Number of previews");
	int streetview_zoom = 2;
	download_sub.add_option("-z,--zoom", streetview_zoom,
		"How much to zoom in street view images, higher numbers "
		"increase resolution");

	auto& render_sub = *app.add_subcommand("render", "Render panoramas");
	std::string initial_id;
	render_sub.add_option("-i,--id", initial_id, "Initial panorama ID")->required();

	CLI11_PARSE(app, argc, argv);

	curl_global_init(CURL_GLOBAL_ALL);
	if(download_sub) {
		std::chrono::time_point<std::chrono::steady_clock> start;
		std::chrono::time_point<std::chrono::steady_clock> stop;

		start = std::chrono::high_resolution_clock::now();

		auto curl_handle = curl_easy_init();

		auto client_id = download_client_id(curl_handle);
		auto preview_document
			= download_preview_document(curl_handle, client_id, num_previews, lat, lng, range);

		stop = std::chrono::high_resolution_clock::now();
		fmt::print("Setup took {}ms\n",
			std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());

		for(auto& panorama_id : extract_panorama_ids(preview_document)) {
			start = std::chrono::high_resolution_clock::now();

			// Get photometa
			auto photometa_document = download_photometa(curl_handle, client_id, panorama_id);

			// Get panorama
			auto tile_surface
				= download_panorama(curl_handle, panorama_id, streetview_zoom, photometa_document);

			std::string filename = fmt::format("tiles/stitched-{}.png", panorama_id);

			// std::cout << "Location: " << extract_location(photometa_document).city_and_state
			//		  << std::endl;
			// for(auto& adjacent : extract_adjacent_panoramas(photometa_document)) {
			//	std::cout << "Adjacent: " << adjacent.id << " " << adjacent.lat << " "
			//			  << adjacent.lng << std::endl;
			// }

			std::filesystem::create_directory("tiles");

			auto tile_data = tile_surface->encodeToData(SkEncodedImageFormat::kPNG, 95);
			std::ofstream outfile(filename, std::ios::out | std::ios::binary);
			outfile.write((const char*)tile_data->bytes(), tile_data->size());
			outfile.close();

			stop = std::chrono::high_resolution_clock::now();
			fmt::print("Downloading {} took {}ms\n", panorama_id,
				std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
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