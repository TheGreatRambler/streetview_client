#pragma once

#include <core/SkImage.h>
#include <core/SkShader.h>

sk_sp<SkShader> get_equirectangular_shader(SkImage* image);