#define RAPIDJSON_HAS_STDSTRING 1

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <GLFW/glfw3.h>
#include <codec/SkCodec.h>
#include <core/SkBitmap.h>
#include <core/SkCanvas.h>
#include <core/SkColor.h>
#include <core/SkFont.h>
#include <core/SkGraphics.h>
#include <core/SkImage.h>
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

#include "equirectangular.hpp"
#include "extract.hpp"
#include "headers.hpp"
#include "parse.hpp"
#include "renderer.hpp"

static size_t write_memory_callback(void* contents, size_t size, size_t nmemb, void* userp) {
	size_t realsize = size * nmemb;
	auto& mem       = *static_cast<std::string*>(userp);
	mem.append(static_cast<char*>(contents), realsize);
	return realsize;
}

std::string download_from_url(
	std::string url, CURL* curl_handle, CURLcode* res, curl_slist* headers) {
	std::string download;
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &download);
	if(headers) {
		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
	}

	// added options that may be required
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L); // redirects
	curl_easy_setopt(curl_handle, CURLOPT_HTTPPROXYTUNNEL,
		1L);                                                   // corp. proxies etc.
	// curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L); // we want it all
	//  curl_easy_setopt(curl_handle, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP |
	//  CURLPROTO_HTTPS);

	*res = curl_easy_perform(curl_handle);

	if(*res != CURLE_OK) {
		std::cerr << "Downloading failed: " << curl_easy_strerror(*res) << std::endl;
	}

	return download;
}

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
	std::string filename;
	render_sub.add_option("-f,--filename", filename, "Equirectangular image filename")->required();

	CLI11_PARSE(app, argc, argv);

	if(download_sub) {
		std::chrono::time_point<std::chrono::steady_clock> start;
		std::chrono::time_point<std::chrono::steady_clock> stop;

		start = std::chrono::high_resolution_clock::now();

		curl_global_init(CURL_GLOBAL_ALL);
		auto curl_handle = curl_easy_init();

		CURLcode res;
		auto main_page_download = download_from_url(
			"https://www.google.com/maps", curl_handle, &res, get_main_page_headers());
		auto client_id = parse_client_id(main_page_download);

		auto photo_preview_download = download_from_url(
			fmt::format("https://www.google.com/maps/preview/"
						"photo?authuser=0&hl=en&gl=us&pb=!1e3!5m54!2m2!1i203!2i100!3m3!2i{}"
						"!"
						"3s{}!5b1!7m42!1m3!1e1!2b0!3e3!1m3!1e2!2b1!3e2!1m3!1e2!2b0!3e3!1m3!"
						"1e8!"
						"2b0!3e3!1m3!1e10!2b0!3e3!1m3!1e10!2b1!3e2!1m3!1e9!2b1!3e2!1m3!"
						"1e10!"
						"2b0!"
						"3e3!1m3!1e10!2b1!3e2!1m3!1e10!2b0!3e4!2b1!4b1!8m0!9b0!11m1!4b1!"
						"6m3!"
						"1s{}"
						"!"
						"7e81!15i11021!9m2!2d{}!3d{}!10d{}",
				num_previews, "CAEIBAgFCAYgAQ", client_id, lng, lat, range),
			curl_handle, &res, NULL);

		// Parse into JSON
		rapidjson::Document preview_document;
		preview_document.Parse(photo_preview_download.substr(4));

		stop = std::chrono::high_resolution_clock::now();
		fmt::print("Setup took {}ms\n",
			std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());

		if(preview_document.IsArray() && preview_document.Size() == 11) {
			if(preview_document[0].IsArray() && preview_document[0].Size() > 0) {
				std::cout << preview_document[0].Size() << " previews" << std::endl;
				for(auto& preview : preview_document[0].GetArray()) {
					if(preview.IsArray() && preview.Size() == 32) {
						if(preview[0].IsString()) {
							start = std::chrono::high_resolution_clock::now();
							auto panorama_id
								= std::string(preview[0].GetString(), preview[0].GetStringLength());
							if(panorama_id.size() != 22) {
								// Not a street view
								continue;
							}

							// Get photometa
							auto photometa_url = fmt::format(
								"https://www.google.com/maps/photometa/"
								"v1?authuser=0&hl=en&gl=us&pb=!1m4!1smaps_sv.tactile!11m2!"
								"2m1!"
								"1b1!2m2!1sen!2sus!3m3!1m2!1e2!2s{}!4m57!1e1!1e2!1e3!1e4!1e5!"
								"1e6!1e8!1e12!2m1!1e1!4m1!1i48!5m1!1e1!5m1!1e2!6m1!1e1!6m1!"
								"1e2!"
								"9m36!1m3!1e2!2b1!3e2!1m3!1e2!2b0!3e3!1m3!1e3!2b1!3e2!1m3!"
								"1e3!"
								"2b0!3e3!1m3!1e8!2b0!3e3!1m3!1e1!2b0!3e3!1m3!1e4!2b0!3e3!1m3!"
								"1e10!2b1!3e2!1m3!1e10!2b0!3e3",
								panorama_id);
							auto photometa_download = download_from_url(
								photometa_url, curl_handle, &res, get_photometa_headers());
							rapidjson::Document photometa_document;
							photometa_document.Parse(photometa_download.substr(4));

							std::string filename
								= fmt::format("tiles/stitched-{}.png", panorama_id);

							// std::cout << "Location: "
							//		  << extract_location(photometa_document).city_and_state
							//		  << std::endl;
							// for(auto& adjacent : extract_adjacent_panoramas(photometa_document))
							// { 	std::cout << "Adjacent: " << adjacent.id << " " << adjacent.lat
							//			  << " " << adjacent.lng << std::endl;
							// }

							auto& tiles_dimensions = photometa_document[1][0][2][2];
							double tiles_height
								= tiles_dimensions[0].GetInt() / 512 / pow(2, 5 - streetview_zoom);
							double tiles_width
								= tiles_dimensions[1].GetInt() / 512 / pow(2, 5 - streetview_zoom);

							sk_sp<SkSurface> tile_surface = SkSurface::MakeRasterN32Premul(
								tiles_width * 512, tiles_height * 512);

							// Start processing
							tile_surface->getCanvas()->clear(SK_ColorWHITE);
							for(int y = 0; y < tiles_height; y++) {
								for(int x = 0; x < tiles_width; x++) {
									auto tile_url = fmt::format(
										"https://streetviewpixels-pa.googleapis.com/v1/"
										"tile?cb_client=maps_sv.tactile&panoid={}&x={}&"
										"y={}&zoom={}&nbt=1&fover=2",
										panorama_id, x, y, streetview_zoom);
									auto tile_download = download_from_url(
										tile_url, curl_handle, &res, get_panorama_headers());

									if(res == CURLE_OK) {
										long http_code = 0;
										curl_easy_getinfo(
											curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
										if(http_code == 200) {
											SkBitmap bitmap;
											std::unique_ptr<SkCodec> jpeg
												= SkCodec::MakeFromData(SkData::MakeWithCopy(
													tile_download.data(), tile_download.size()));
											if(!jpeg) {
												std::cerr << "Could not get image from jpeg data: "
														  << tile_download << std::endl;
											}
											SkImageInfo info = jpeg->getInfo().makeColorType(
												kBGRA_8888_SkColorType);
											bitmap.allocPixels(info);
											jpeg->getPixels(
												info, bitmap.getPixels(), bitmap.rowBytes());
											bitmap.setImmutable();

											tile_surface->getCanvas()->drawImage(
												bitmap.asImage(), 512 * x, 512 * y);
										} else {
											std::cout << "Error code " << http_code << std::endl;
										}
									}
								}
							}

							std::filesystem::create_directory("tiles");
							SkFILEWStream dest(filename.c_str());
							SkPngEncoder::Options options;
							options.fZLibLevel = 9;

							SkBitmap bitmap;
							bitmap.allocPixels(
								SkImageInfo::Make(tile_surface->width(), tile_surface->height(),
									SkColorType::kRGB_888x_SkColorType,
									SkAlphaType::kOpaque_SkAlphaType),
								0);

							tile_surface->getCanvas()->readPixels(bitmap, 0, 0);
							SkPixmap src;
							bitmap.peekPixels(&src);
							if(!SkPngEncoder::Encode(&dest, src, options)) {
								std::cout << fmt::format("Could not render {}", panorama_id)
										  << std::endl;
							}

							stop = std::chrono::high_resolution_clock::now();
							fmt::print("Downloading {} took {}ms\n", panorama_id,
								std::chrono::duration_cast<std::chrono::milliseconds>(stop - start)
									.count());
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

		curl_easy_cleanup(curl_handle);
		curl_global_cleanup();
	} else if(render_sub) {
		EquirectangularWindow window;
		window.SetImage(SkImage::MakeFromEncoded(SkData::MakeFromFileName(filename.c_str())));
		window.PrepareWindow();
		window.PrepareShader();
		while(!window.ShouldClose()) {
			window.DrawFrame();
		}
	}

	return 0;
}