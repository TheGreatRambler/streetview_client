#pragma once

#define SK_ENABLE_SKSL

#include <core/SkSurface.h>
#include <effects/SkRuntimeEffect.h>

#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

class EquirectangularWindow {
public:
	void PrepareWindow();
	void SetImage(sk_sp<SkImage> image) {
		current_image = image;
	}
	void DrawFrame();
	void PrepareShader();
	void UpdateCamera(double pitch, double yaw, double hfov);

	bool ShouldClose() {
		return glfwWindowShouldClose(window);
	}

private:
	void RenderPanorama();

	sk_sp<SkSurface> surface;
	GLFWwindow* window;
	sk_sp<GrDirectContext> direct_context;
	sk_sp<SkImage> current_image;
	SkRuntimeShaderBuilder* shader_builder = nullptr;
	sk_sp<SkRuntimeEffect> shader_effect   = nullptr;
	sk_sp<SkImageFilter> spherical_filter;
	int width;
	int height;
	int frame = 0;

	GLint aspect_ratio_location;
	GLint psi_location;
	GLint theta_location;
	GLint f_location;
	GLint h_location;
	GLint v_location;
	GLint vo_location;
	GLint rot_location;
	GLint background_color_location;
	GLint texture_location;
};