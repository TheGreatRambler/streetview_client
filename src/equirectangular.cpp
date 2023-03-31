#include "equirectangular.hpp"

#define SK_ENABLE_SKSL

#include <core/SkCanvas.h>
#include <core/SkData.h>
#include <core/SkStream.h>
#include <core/SkSurface.h>
#include <effects/SkRuntimeEffect.h>
#include <effects/SkShaderMaskFilter.h>
#include <encode/SkPngEncoder.h>
#include <gpu/GrBackendSurface.h>
#include <gpu/GrDirectContext.h>

#include <iostream>

sk_sp<SkShader> get_equirectangular_shader(SkImage* image) {
	// clang-format off
  const char *sksl =
    "uniform shader image;"
    "half4 main(float2 coord) {"
    "  coord.x += sin(coord.y / 3) * 4;" // Displace each row by up to 4 pixels
    "  return image.eval(coord);"
    "}";
	// clang-format on

	// Turn `image` into an SkShader:
	sk_sp<SkShader> imageShader = image->makeShader(SkSamplingOptions(SkFilterMode::kLinear));

	// Parse the SkSL, and create an SkRuntimeEffect object:
	auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(sksl));

	// SkRuntimeEffect::makeShader expects an SkSpan<ChildPtr>, one per `uniform
	// shader`:
	SkRuntimeEffect::ChildPtr children[] = { imageShader };

	// Create an SkShader from our SkSL, with `imageShader` bound to `image`:
	sk_sp<SkShader> myShader = effect->makeShader(/*uniforms=*/nullptr,
		/*children=*/ { children, 1 });

	return myShader;
}