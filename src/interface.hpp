#pragma once

#define SK_ENABLE_SKSL

#include <core/SkSurface.h>
#include <effects/SkRuntimeEffect.h>

#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <iostream>
#include <memory>
#include <vector>

#include "extract.hpp"
#include "preloader.hpp"

class InterfaceWindow {
public:
	InterfaceWindow(std::string initial_panorama_id, CURL* curl_handle);

	bool PrepareWindow();
	void DrawFrame();
	void ChangePanorama(std::string id);
	void PrepareShader();
	Panorama& GetClosestAdjacent();

	bool ShouldClose() {
		return glfwWindowShouldClose(window);
	}

	struct glfw_events_s {
		bool currently_dragging = false;
		// The location you have started dragging in
		double drag_start_x;
		double drag_start_y;
		// The initial drag location
		double screen_offset_start_x;
		double screen_offset_start_y;
		double screen_offset_x;
		double screen_offset_y;
		// Zoom
		double zoom = 5.0;
	};
	glfw_events_s glfw_events;

private:
	void RenderPanorama();
	void RenderMap();

	// Render variables
	sk_sp<SkSurface> surface;
	GLFWwindow* window;
	sk_sp<GrDirectContext> direct_context;
	SkRuntimeShaderBuilder* shader_builder = nullptr;
	sk_sp<SkRuntimeEffect> shader_effect   = nullptr;
	sk_sp<SkImageFilter> spherical_filter;
	std::chrono::steady_clock::time_point timing_start;
	int width;
	int height;
	int frame = 0;

	// Panorama variables
	CURL* curl_handle;
	PanoramaPreloader preloader;
	std::shared_ptr<PanoramaDownload> panorama_info;
	Panorama current_panorama;
	std::vector<Panorama> adjacent;
	float yaw;
	float pitch;
};