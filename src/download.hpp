#pragma once

#include <core/SkCanvas.h>
#include <core/SkSurface.h>
#include <curl/curl.h>
#include <rapidjson/document.h>

#include <string>

#include "extract.hpp"

std::string download_from_url(
	std::string url, CURL* curl_handle, CURLcode* res, curl_slist* headers);
std::string download_client_id(CURL* curl_handle);
rapidjson::Document download_preview_document(
	CURL* curl_handle, std::string client_id, int num_previews, double lat, double lng, int range);
rapidjson::Document download_photometa(
	CURL* curl_handle, std::string client_id, std::string panorama_id);
sk_sp<SkImage> download_panorama(CURL* curl_handle, std::string panorama_id, int streetview_zoom,
	rapidjson::Document& photmeta_document);
std::vector<Panorama> get_infos(
	CURL* curl_handle, std::string client_id, std::vector<std::string>& ids);