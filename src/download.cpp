#include "download.hpp"

#define MAPS_PREVIEW_ID "CAEIBAgFCAYgAQ"

#include <core/SkData.h>
#include <core/SkImage.h>
#include <fmt/format.h>

#include <iostream>

#include "extract.hpp"
#include "headers.hpp"
#include "parse.hpp"

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
		1L); // corp. proxies etc.
	// curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L); // we want it all
	//  curl_easy_setopt(curl_handle, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP |
	//  CURLPROTO_HTTPS);

	*res = curl_easy_perform(curl_handle);

	if(*res != CURLE_OK) {
		std::cerr << "Downloading failed: " << curl_easy_strerror(*res) << std::endl;
	}

	return download;
}

std::string download_client_id(CURL* curl_handle) {
	CURLcode res;
	auto main_page_download = download_from_url(
		"https://www.google.com/maps", curl_handle, &res, get_main_page_headers());
	auto client_id = parse_client_id(main_page_download);
	return client_id;
}

rapidjson::Document download_preview_document(
	CURL* curl_handle, std::string client_id, int num_previews, double lat, double lng, int range) {
	CURLcode res;
	auto photo_preview_download = download_from_url(
		fmt::format(
			"https://www.google.com/maps/rpc/photo/listentityphotos?authuser=0&hl=en&gl=us&pb=!1e3!5m54!2m2!1i203!2i100!3m3!2i{}"
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
			num_previews, MAPS_PREVIEW_ID, client_id, lng, lat, range),
		curl_handle, &res, NULL);

	// Parse into JSON
	rapidjson::Document preview_document;
	preview_document.Parse(photo_preview_download.substr(4));

	return preview_document;
}

rapidjson::Document download_photometa(
	CURL* curl_handle, std::string client_id, std::string panorama_id) {

	CURLcode res;
	auto photometa_url = fmt::format("https://www.google.com/maps/photometa/"
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
	auto photometa_download
		= download_from_url(photometa_url, curl_handle, &res, get_photometa_headers());
	rapidjson::Document photometa_document;
	photometa_document.Parse(photometa_download.substr(4));

	return photometa_document;
}

sk_sp<SkImage> download_panorama(CURL* curl_handle, std::string panorama_id, int streetview_zoom,
	rapidjson::Document& photmeta_document) {
	auto [tiles_width, tiles_height] = extract_tiles_dimensions(photmeta_document, streetview_zoom);

	sk_sp<SkSurface> tile_surface
		= SkSurface::MakeRasterN32Premul(tiles_width * 512, tiles_height * 512);

	// Start processing
	tile_surface->getCanvas()->clear(SK_ColorWHITE);
	for(int y = 0; y < tiles_height; y++) {
		for(int x = 0; x < tiles_width; x++) {
			// Download the specific tile
			// Each tile takes around ~40ms to download
			CURLcode res;
			auto tile_url = fmt::format(
				"https://streetviewpixels-pa.googleapis.com/v1/tile?cb_client=maps_sv.tactile&panoid={}&x={}&"
				"y={}&zoom={}&nbt=1&fover=2",
				panorama_id, x, y, streetview_zoom);
			auto tile_download
				= download_from_url(tile_url, curl_handle, &res, get_panorama_headers());

			if(res == CURLE_OK) {
				long http_code = 0;
				curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
				if(http_code == 200) {
					// Construct an image from the data
					// This is less than 1ms
					auto image = SkImage::MakeFromEncoded(
						SkData::MakeWithoutCopy(tile_download.data(), tile_download.size()));
					tile_surface->getCanvas()->drawImage(image, 512 * x, 512 * y);
				} else {
					std::cout << "Error code " << http_code << std::endl;
				}
			}
		}
	}

	return tile_surface->makeImageSnapshot();
}

std::vector<Panorama> get_infos(
	CURL* curl_handle, std::string client_id, std::vector<std::string>& ids) {
	std::vector<Panorama> infos;
	for(auto& id : ids) {
		// Get photometa
		auto photometa_document = download_photometa(curl_handle, client_id, id);

		// Get info
		auto panorama_info = extract_info(photometa_document);
		infos.push_back(panorama_info);
	}
	return infos;
}