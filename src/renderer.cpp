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

void EquirectangularWindow::PrepareWindow() {
	/* Initialize the library */
	if(!glfwInit()) {
		// return nullptr;
	}

#ifdef __APPLE__
	/* We need to explicitly ask for a 3.2 context on OS X */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

	/* Create a windowed mode window and its OpenGL context */
	glfwWindowHint(GLFW_MAXIMIZED, 1);
	window = glfwCreateWindow(1, 1, "Hello World", NULL, NULL);
	if(!window) {
		glfwTerminate();
		// return nullptr;
	}

	glfwGetFramebufferSize(window, &width, &height);

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

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
}

void EquirectangularWindow::DrawFrame() {
	RenderPanorama();
	surface->getCanvas()->flush();
	glfwSwapBuffers(window);
	glfwWaitEvents();
	frame++;
}

void EquirectangularWindow::RenderPanorama() {
	if(shader_builder) {
		// Set view resolution
		shader_builder->uniform("u_viewResolution")
			= SkV2 { (float)surface->width(), (float)surface->height() };

		// Set view resolution
		float pitch = 3.14159;
		shader_builder->uniform("u_pitch").set(&pitch, 1);
		float yaw = (float)frame / 100;
		shader_builder->uniform("u_yaw").set(&yaw, 1);
		float fov_h = 1.6;
		shader_builder->uniform("u_fovH").set(&fov_h, 1);
		float fov_v = 1.04;
		shader_builder->uniform("u_fovV").set(&fov_v, 1);

		SkPaint shader_paint;
		shader_paint.setShader(shader_builder->makeShader());
		surface->getCanvas()->drawPaint(shader_paint);
	}
}

#define PI 3.14159265358979323846264
void EquirectangularWindow::PrepareShader() {
	const char* sksl_src = R"(
// Handle 8 images at once, each one max 2048x2048
uniform shader image;

uniform vec2 u_imageResolution;
uniform vec2 u_viewResolution;

uniform float u_pitch;
uniform float u_yaw;
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
    vec2 uv = fragCoord.xy * 2./u_viewResolution.xy - 1.;
	
    //to spherical
    vec3 camDir = normalize(vec3(uv.xy * vec2(tan(0.5 * u_fovH), tan(0.5 * u_fovV)), 1.0));
    
    //camRot is angle vec in rad
    //vec3 camRot = vec3( ((iMouse.xy / iResolution.xy) - 0.5) * vec2(2.0 * PI,  PI), 0.);
	vec3 camRot = vec3(u_yaw, u_pitch, 0.0);
    
    //rotate
    vec3 rd = normalize(rotateXY(camDir, camRot.yx));
    
    //radial azmuth polar
    vec2 texCoord = 1 - vec2(atan(rd.z, rd.x) + PI, acos(-rd.y)) / vec2(2.0 * PI, PI);
	vec2 imageCoord = texCoord * u_imageResolution;

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
		= SkV2 { (float)current_image->width(), (float)current_image->height() };

	// Set one time image
	shader_builder->child("image")
		= current_image->makeShader(SkSamplingOptions(SkFilterMode::kLinear));
}

void EquirectangularWindow::UpdateCamera(double pitch, double yaw, double hfov) { }