#include "interface.hpp"

#define SK_GL 1
#define SK_GANESH 1
#define SK_ENABLE_SKSL 1
#define PI 3.14159265358979323846264f

#include <core/SkBitmap.h>
#include <core/SkCanvas.h>
#include <core/SkColorSpace.h>
#include <core/SkGraphics.h>
#include <core/SkImageFilter.h>
#include <core/SkPixmap.h>
#include <core/SkSurface.h>
#include <effects/SkImageFilters.h>
#include <fmt/format.h>
#include <gpu/GrBackendSurface.h>
#include <gpu/GrDirectContext.h>
#include <gpu/gl/GrGLInterface.h>

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "download.hpp"

InterfaceWindow::InterfaceWindow(std::string initial_panorama_id, CURL* curl_handle) {
	// Start preloader
	preloader.SetClientId(download_client_id(curl_handle));
	preloader.SetZoom(1);
	preloader.SetCurlHandle(curl_handle);
	preloader.Start(5);

	// Set initial panorama
	ChangePanorama(initial_panorama_id);
}

bool InterfaceWindow::PrepareWindow() {
	if(!glfwInit()) {
		return false;
	}

#ifdef __APPLE__
	/* We need to explicitly ask for a 3.2 context on OS X */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);
#endif

	/* Create a windowed mode window and its OpenGL context */
	glfwWindowHint(GLFW_MAXIMIZED, 1);
	window = glfwCreateWindow(1, 1, "Streetview Client", NULL, NULL);
	if(!window) {
		glfwTerminate();
		return false;
	}

	glfwGetFramebufferSize(window, &width, &height);

	glfwSetCursorPosCallback(
		window, *[](GLFWwindow* window, double x, double y) {
			InterfaceWindow* renderer = (InterfaceWindow*)glfwGetWindowUserPointer(window);
			if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_RELEASE) {
				if(!renderer->glfw_events.currently_dragging) {
					renderer->glfw_events.drag_start_x = x;
					renderer->glfw_events.drag_start_y = y;
					renderer->glfw_events.screen_offset_start_x
						= renderer->glfw_events.screen_offset_x;
					renderer->glfw_events.screen_offset_start_y
						= renderer->glfw_events.screen_offset_y;
					renderer->glfw_events.currently_dragging = true;
				} else {
					double shift_x = x - renderer->glfw_events.drag_start_x;
					double shift_y = y - renderer->glfw_events.drag_start_y;
					renderer->glfw_events.screen_offset_x
						= renderer->glfw_events.screen_offset_start_x + shift_x;
					renderer->glfw_events.screen_offset_y
						= renderer->glfw_events.screen_offset_start_y - shift_y;
				}
			} else {
				renderer->glfw_events.currently_dragging = false;
			}
		});
	glfwSetMouseButtonCallback(
		window, *[](GLFWwindow* window, int button, int action, int mods) {
			InterfaceWindow* renderer = (InterfaceWindow*)glfwGetWindowUserPointer(window);

			if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
				double x;
				double y;
				glfwGetCursorPos(window, &x, &y);
				// Switch panorama if possible
				renderer->SwitchToAdjacent(x * 2, y * 2);
			}
		});
	glfwSetScrollCallback(
		window, *[](GLFWwindow* window, double x_offset, double y_offset) {
			InterfaceWindow* renderer = (InterfaceWindow*)glfwGetWindowUserPointer(window);
			renderer->glfw_events.zoom += y_offset / 3.0;
			if(renderer->glfw_events.zoom < 4.0) {
				renderer->glfw_events.zoom = 4.0;
			} else if(renderer->glfw_events.zoom > 15.0) {
				renderer->glfw_events.zoom = 15.0;
			}
		});
	glfwSetKeyCallback(
		window, *[](GLFWwindow* window, int key, int scancode, int action, int mods) {
			InterfaceWindow* renderer = (InterfaceWindow*)glfwGetWindowUserPointer(window);
			if(key == GLFW_KEY_UP && action == GLFW_PRESS) {
				// TODO this heuristic doesn't work
				renderer->ChangePanorama(renderer->GetClosestAdjacent().id);
			}
		});

	// Make the window's context current
	glfwMakeContextCurrent(window);
	// Force fast rendering, probably 120fps
	glfwSwapInterval(0);

	// Set the user pointer
	glfwSetWindowUserPointer(window, this);

	auto interface = GrGLMakeNativeInterface();
	direct_context = GrDirectContext::MakeGL(interface);

	GrGLFramebufferInfo framebufferInfo;
	framebufferInfo.fFBOID = 0; // assume default framebuffer
	// We are always using OpenGL and we use RGBA8 internal format for both RGBA and BGRA configs in
	// OpenGL.
	//(replace line below with this one to enable correct color spaces) framebufferInfo.fFormat =
	// GL_SRGB8_ALPHA8;
	framebufferInfo.fFormat = GL_RGBA8;

	SkColorType colorType = kRGBA_8888_SkColorType;
	GrBackendRenderTarget backendRenderTarget(width, height,
		0, // sample count
		0, // stencil bits
		framebufferInfo);

	surface = SkSurface::MakeFromBackendRenderTarget(direct_context.get(), backendRenderTarget,
		kBottomLeft_GrSurfaceOrigin, colorType, SkColorSpace::MakeSRGB(), nullptr);

	timing_start = std::chrono::high_resolution_clock::now();
	return true;
}

void InterfaceWindow::DrawFrame() {
	RenderPanorama();
	RenderMap();
	surface->getCanvas()->flush();
	glfwSwapBuffers(window);
	glfwPollEvents();
	frame++;

	// Start queueing
	// QueueCloseAdjacent();
	// fmt::print("FPS: {}\n", (double)frame
	//							/ (std::chrono::high_resolution_clock::now() - timing_start).count()
	//							* 1000.0);
}

void InterfaceWindow::ChangePanorama(std::string id) {
	panorama_info    = preloader.GetPanorama(id, true);
	current_panorama = extract_info(panorama_info->photometa);
	adjacent         = extract_adjacent_panoramas(panorama_info->photometa);
	PrepareShader();
}

void InterfaceWindow::RenderPanorama() {
	if(shader_builder) {
		// Set view resolution
		shader_builder->uniform("u_viewResolution")
			= SkV2 { (float)surface->width(), (float)surface->height() };

		// Calculate yaw and pitch in C++
		// This is important because it is needed later for streetview navigation
		auto normalized_x
			= (float)glfw_events.screen_offset_x / (float)surface->width() * 2.0f - 1.0f;
		auto normalized_y
			= (float)glfw_events.screen_offset_y / (float)surface->height() * 2.0f - 1.0f;
		yaw   = normalized_x * PI * 0.5f;
		pitch = normalized_y * PI * 0.33f + -PI * 0.65f;

		// Calculate corrected yaw
		corrected_yaw = std::fmod(std::fmod(yaw - PI, 2 * PI) + 2 * PI, 2 * PI);

		// Set mouse rotation
		shader_builder->uniform("u_rotation") = SkV2 { yaw, pitch };

		// Correct FOV formula
		// Vertical FOV = Initial FOV / Zoom
		// Horizontal FOV = 2 * arctan(tan(Vertical FOV / 2) * Aspect Ratio)
		double fov  = 180 / std::pow(2, glfw_events.zoom);
		double zoom = glfw_events.zoom;

		float fov_v = fov / zoom;
		shader_builder->uniform("u_fovV").set(&fov_v, 1);
		float fov_h
			= 2 * std::atan(std::tan(fov_v / 2) * (float)surface->width() / surface->height());
		shader_builder->uniform("u_fovH").set(&fov_h, 1);

		SkPaint shader_paint;
		shader_paint.setShader(shader_builder->makeShader());
		surface->getCanvas()->drawPaint(shader_paint);
	}
}

void InterfaceWindow::RenderMap() {
	// Render in bottom right, 200 by 200
	int start_x = surface->width() - map_width;
	int start_y = surface->height() - map_height;

	// Draw the background
	SkPaint background_paint;
	background_paint.setColor(SkColorSetARGB(255, 255, 255, 255));
	surface->getCanvas()->drawRect(
		SkRect::MakeXYWH(start_x, start_y, map_width, map_height), background_paint);

	// Draw the adjacent
	SkPaint adjacent_panorama_paint;
	for(auto& panorama : adjacent) {
		adjacent_panorama_paint.setColor(
			SkColorSetARGB(255, PanoramaClosenessHeuristic(panorama) * 100.0, 0, 0));

		surface->getCanvas()->drawCircle(GetMapPoint(panorama), 8, adjacent_panorama_paint);
	}

	// Draw the current panorama
	SkPaint current_panorama_paint;
	current_panorama_paint.setColor(SkColorSetARGB(255, 0, 255, 0));
	surface->getCanvas()->drawCircle(
		start_x + map_width / 2, start_y + map_height / 2, 12, current_panorama_paint);
}

SkPoint InterfaceWindow::GetMapPoint(Panorama& adjacent) {
	int start_x = surface->width() - map_width;
	int start_y = surface->height() - map_height;
	return SkPoint::Make(
		start_x + map_width / 2 + (adjacent.lat - current_panorama.lat) * map_scale,
		start_y + map_height / 2 + (adjacent.lng - current_panorama.lng) * map_scale);
}

void InterfaceWindow::PrepareShader() {
	const char* sksl_src = R"(
	// Handle 8 images at once, each one max 2048x2048
	uniform shader image;

	uniform vec2 u_imageResolution;
	uniform vec2 u_viewResolution;
	uniform vec2 u_rotation;

	//uniform float u_pitch;
	//uniform float u_yaw;
	uniform float u_fovH;
	uniform float u_fovV;

	const float PI = 3.14159265358979323846264;
	const float PI2 = 3.14159265358979323846264 * 2.0;
	const float PI_2 = 3.14159265358979323846264 * 0.5;

	vec3 rotateXY(vec3 p, vec2 angle) {
	    vec2 c = cos(angle), s = sin(angle);
	    p = vec3(p.x, c.x*p.y + s.x*p.z, -s.x*p.y + c.x*p.z);
	    return vec3(c.y*p.x + s.y*p.z, p.y, -s.y*p.x + c.y*p.z);
	}

	float4 main(float2 fragCoord) {
		// Place 0,0 in center from -1 to 1 ndc
	    vec2 uv = fragCoord * 2.0 / u_viewResolution - 1.0;

	    // Spherical
	    vec3 camDir = normalize(vec3(uv * vec2(tan(0.5 * u_fovH), tan(0.5 * u_fovV)), 1.0));
	
	    // Rotate
	    vec3 rd = normalize(rotateXY(camDir, u_rotation.yx));
	
	    // Radial azmuth polar
	    vec2 texCoord = vec2(atan(rd.z, rd.x) + PI, acos(-rd.y)) / vec2(2.0 * PI, PI);
		// Y is flipped but X is not
		vec2 imageCoord = vec2(texCoord.x, 1 - texCoord.y) * u_imageResolution;

		return image.eval(imageCoord);
	}
		)";

	auto [effect, errorText] = SkRuntimeEffect::MakeForShader(SkString(sksl_src));
	if(!effect) {
		fprintf(stdout, "sksl didn't compile: %s", errorText.c_str());
		return;
	}
	shader_effect = effect;

	if(shader_builder) {
		delete shader_builder;
	}
	shader_builder = new SkRuntimeShaderBuilder(std::move(effect));

	// Set one time image resolution
	shader_builder->uniform("u_imageResolution")
		= SkV2 { (float)panorama_info->image->width(), (float)panorama_info->image->height() };

	// Set image
	shader_builder->child("image")
		= panorama_info->image->makeShader(SkSamplingOptions(SkFilterMode::kLinear));
}

Panorama& InterfaceWindow::GetClosestAdjacent() {
	Panorama closest_panorama;
	double closest_difference = 1000;
	for(auto& panorama : adjacent) {
		auto heuristic = PanoramaClosenessHeuristic(panorama);

		if(heuristic < closest_difference) {
			closest_difference = heuristic;
			closest_panorama   = panorama;
		}
	}
	return closest_panorama;
}

void InterfaceWindow::SwitchToAdjacent(double x, double y) {
	int start_x = surface->width() - map_width;
	int start_y = surface->height() - map_height;
	// Check if click is on map
	if(x > start_x && x < surface->width() && y > start_y && y < surface->height()) {
		// Find closest
		Panorama closest_panorama;
		double closest_dist = 1000;
		for(auto& panorama : adjacent) {
			auto loc  = GetMapPoint(panorama);
			auto dist = std::sqrt(std::pow(loc.x() - x, 2) + std::pow(loc.y() - y, 2));

			if(dist < closest_dist) {
				closest_dist     = dist;
				closest_panorama = panorama;
			}
		}

		// Switch to it
		ChangePanorama(closest_panorama.id);
	}
}

void InterfaceWindow::QueueCloseAdjacent() {
	auto sorted_adjacent = adjacent;
	std::sort(sorted_adjacent.begin(), sorted_adjacent.end(), [this](Panorama& a, Panorama& b) {
		return PanoramaClosenessHeuristic(b) < PanoramaClosenessHeuristic(a);
	});

	// Only use first half
	for(int i = 0; i < sorted_adjacent.size() / 2; i++) {
		preloader.QueuePanorama(sorted_adjacent[i].id);
	}
}

double InterfaceWindow::PanoramaClosenessHeuristic(Panorama& adjacent) {
	auto angle
		= std::atan2(-(adjacent.lng - current_panorama.lng), adjacent.lat - current_panorama.lat)
		  + PI;
	return std::abs(angle - corrected_yaw)
		   + std::sqrt(std::pow(adjacent.lat - current_panorama.lat, 2)
					   + std::pow(adjacent.lng - current_panorama.lng, 2))
				 * 3000.0;
}