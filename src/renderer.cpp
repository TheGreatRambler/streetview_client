#include "renderer.hpp"

#define SK_GL 1
#define SK_GANESH 1
#define SK_ENABLE_SKSL 1

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
#include <ui.h>

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
	window = glfwCreateWindow(1, 1, "Hello World", NULL, NULL);
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
				// Load another panorama
				renderer->SetImage(SkImage::MakeFromEncoded(
					SkData::MakeFromFileName("tiles/stitched-NRQ3LOFsRR15hQaPleVRug.png")));
				renderer->PrepareShader();
			}
		});

	// Make the window's context current
	glfwMakeContextCurrent(window);

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

	//(replace line below with this one to enable correct color spaces) sSurface =
	// SkSurface::MakeFromBackendRenderTarget(sContext, backendRenderTarget,
	// kBottomLeft_GrSurfaceOrigin, colorType, SkColorSpace::MakeSRGB(), nullptr).release();
	surface = SkSurface::MakeFromBackendRenderTarget(direct_context.get(), backendRenderTarget,
		kBottomLeft_GrSurfaceOrigin, colorType, nullptr, nullptr);

	timing_start = std::chrono::high_resolution_clock::now();
	return true;
}

void InterfaceWindow::DrawFrame() {
	RenderPanorama();
	surface->getCanvas()->flush();
	glfwSwapBuffers(window);
	glfwPollEvents();
	frame++;

	// fmt::print("FPS: {}\n", (double)frame
	//							/ (std::chrono::high_resolution_clock::now() - timing_start).count()
	//							* 1000.0);
}

void InterfaceWindow::RenderPanorama() {
	if(shader_builder) {
		// Set view resolution
		shader_builder->uniform("u_viewResolution")
			= SkV2 { (float)surface->width(), (float)surface->height() };

		// Set mouse location from drag
		shader_builder->uniform("u_mouse")
			= SkV2 { (float)glfw_events.screen_offset_x, (float)glfw_events.screen_offset_y };

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

#define PI 3.14159265358979323846264
void InterfaceWindow::PrepareShader() {
	const char* sksl_src = R"(
// Handle 8 images at once, each one max 2048x2048
uniform shader image;

uniform vec2 u_imageResolution;
uniform vec2 u_viewResolution;
uniform vec2 u_mouse;

//uniform float u_pitch;
//uniform float u_yaw;
uniform float u_fovH;
uniform float u_fovV;

const float PI = 3.14159265358979323846264;
const float PI2 = 3.14159265358979323846264 * 2.0;
const float PI_2 = 3.14159265358979323846264 * 0.5;

//tools
vec3 rotateXY(vec3 p, vec2 angle) {
    vec2 c = cos(angle), s = sin(angle);
    p = vec3(p.x, c.x*p.y + s.x*p.z, -s.x*p.y + c.x*p.z);
    return vec3(c.y*p.x + s.y*p.z, p.y, -s.y*p.x + c.y*p.z);
}

float4 main(float2 fragCoord) {
	//place 0,0 in center from -1 to 1 ndc
    vec2 uv = fragCoord * 2.0 / u_viewResolution - 1.0;
	
    //to spherical
    vec3 camDir = normalize(vec3(uv * vec2(tan(0.5 * u_fovH), tan(0.5 * u_fovV)), 1.0));
    
    //camRot is angle vec in rad
	vec2 normalizedMouse = u_mouse / u_viewResolution * 2.0 - 1.0;
    vec2 camRot = normalizedMouse * vec2(PI / 2.0,  PI / 3.0) + vec2(0.0, -PI * 0.65);
	//vec2 camRot = vec2((u_mouse.x / u_viewResolution.x - 0.5) * 2.0 * PI, (0.5 - u_mouse.y / u_viewResolution.y) * PI + PI * 0.5);
	//vec2 camRot = vec2(u_yaw, u_pitch);
    
    //rotate
    vec3 rd = normalize(rotateXY(camDir, camRot.yx));
    
    //radial azmuth polar
    vec2 texCoord = 1 - vec2(atan(rd.z, rd.x) + PI, acos(-rd.y)) / vec2(2.0 * PI, PI);
	vec2 imageCoord = texCoord * u_imageResolution;

	return image.eval(imageCoord);
	//return vec4(u_mouse.x / u_viewResolution.x, 0.0, 0.0, 1.0);
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
		= SkV2 { (float)current_image->width(), (float)current_image->height() };

	// Set one time image
	shader_builder->child("image")
		= current_image->makeShader(SkSamplingOptions(SkFilterMode::kLinear));
}

void InterfaceWindow::UpdateCamera(double pitch, double yaw, double hfov) { }