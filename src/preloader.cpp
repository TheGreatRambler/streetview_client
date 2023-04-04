#include "preloader.hpp"

#include <chrono>
#include <iostream>

#include "download.hpp"

void PanoramaPreloader::Start(int num_threads) {
	// Create all the threads
	for(int i = 0; i < num_threads; i++) {
		threads.push_back(std::thread(&PanoramaPreloader::PanoramaThread, this));
	}
}

void PanoramaPreloader::QueuePanorama(std::string id) {
	std::scoped_lock lock { queued_panoramas_m, panoramas_m };
	// Panorama must not already be downloaded and the queue must be smaller than 100
	if(!panoramas.count(id) && queued_panoramas.size() < 100) {
		queued_panoramas.emplace_back(id);
	}
}

std::shared_ptr<PanoramaDownload> PanoramaPreloader::GetPanorama(std::string id, bool force) {
	panoramas_m.lock();
	bool have_panorama = panoramas.count(id);

	if(have_panorama) {
		auto info = panoramas[id];
		panoramas_m.unlock();
		return info;
	} else {
		panoramas_m.unlock();
		if(force) {
			// Download regardless on the current thread
			auto info = DownloadPanorama(id, curl_handle);
			return info;
		} else {
			// Don't force download
			return nullptr;
		}
	}
}

void PanoramaPreloader::PanoramaThread() {
	CURL* curl_handle = curl_easy_init();

	while(run_threads) {
		std::this_thread::sleep_for(std::chrono::milliseconds(16));

		queued_panoramas_m.lock();
		if(queued_panoramas.empty()) {
			queued_panoramas_m.unlock();
			continue;
		}
		std::string id = queued_panoramas.front();
		queued_panoramas.pop_front();
		queued_panoramas_m.unlock();

		panoramas_m.lock();
		if(panoramas.count(id)) {
			// Ignore this panorama, we already downloaded it
			panoramas_m.unlock();
			continue;
		}
		panoramas_m.unlock();

		auto info = DownloadPanorama(id, curl_handle);

		panoramas_m.lock();
		panoramas[id] = info;
		panoramas_m.unlock();
	}

	curl_easy_cleanup(curl_handle);
}

std::shared_ptr<PanoramaDownload> PanoramaPreloader::DownloadPanorama(
	std::string id, CURL* handle) {
	std::scoped_lock lock { info_m };

	// Get photometa
	auto photmeta_document = download_photometa(handle, client_id, id);

	// Get panorama
	auto image = download_panorama(handle, id, streetview_zoom, photmeta_document);

	auto info   = std::make_shared<PanoramaDownload>();
	info->id    = id;
	info->image = image;
	info->photometa.Swap(photmeta_document);
	return info;
}