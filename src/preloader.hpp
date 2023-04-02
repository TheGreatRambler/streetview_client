#pragma once

#include <core/SkImage.h>
#include <curl/curl.h>
#include <rapidjson/document.h>

#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

struct PanoramaDownload {
	sk_sp<SkImage> image;
	std::string id;
	rapidjson::Document photometa;
};

class PanoramaPreloader {
public:
	void Start(int num_threads);

	void QueuePanorama(std::string id);
	void SetZoom(int zoom) {
		std::scoped_lock lock { info_m };
		streetview_zoom = zoom;
	}
	void SetClientId(std::string id) {
		std::scoped_lock lock { info_m };
		client_id = id;
	}
	void SetCurlHandle(CURL* handle) {
		curl_handle = handle;
	}
	std::shared_ptr<PanoramaDownload> GetPanorama(std::string id, bool force);

private:
	void PanoramaThread();
	std::shared_ptr<PanoramaDownload> DownloadPanorama(std::string id, CURL* handle);

	CURL* curl_handle;

	std::deque<std::string> queued_panoramas;
	std::mutex queued_panoramas_m;
	std::unordered_map<std::string, std::shared_ptr<PanoramaDownload>> panoramas;
	std::mutex panoramas_m;

	bool run_threads = true;
	std::vector<std::thread> threads;

	int streetview_zoom = 2;
	std::string client_id;
	std::mutex info_m;
};