cmake_minimum_required(VERSION 3.10)
set(STREETVIEW_CLIENT_VERSION "0.0.1")
project(streetview_client VERSION ${STREETVIEW_CLIENT_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_PREFIX_PATH "/opt/homebrew")

set(
  CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -Wno-sign-compare -Wno-deprecated-declarations -Wno-unused-parameter -Wno-extern-initializer -Wno-deprecated-enum-enum-conversion"
)

if(CMAKE_BUILD_TYPE EQUAL "DEBUG")
	add_link_options(-g -O0)
endif()

# fmt for some formatting
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt ${CMAKE_CURRENT_BINARY_DIR}/third_party/fmt)

# Libcurl for downloading files
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/curl ${CMAKE_CURRENT_BINARY_DIR}/third_party/curl)

# CLI11 for better command line support
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/CLI11 ${CMAKE_CURRENT_BINARY_DIR}/third_party/CLI11)

# Rapidjson for some endpoints
set(RAPIDJSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rapidjson/include)
set(RAPIDJSON_BUILD_EXAMPLES OFF)
set(RAPIDJSON_BUILD_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/rapidjson ${CMAKE_CURRENT_BINARY_DIR}/third_party/rapidjson)
string(REPLACE "-march=native" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

# Skia for rendering fisheye
set(SKIA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/skia)
set(SKIA_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/third_party/skia)
execute_process(
	COMMAND python tools/git-sync-deps
	WORKING_DIRECTORY ${SKIA_DIR})
if(ANDROID)
	
elseif(WIN32)
	
else()
	# MacOS
	execute_process(
		COMMAND bin/gn gen ${SKIA_BUILD_DIR} "--args=target_cpu=\"arm64\" is_official_build=true skia_use_system_libjpeg_turbo=false skia_use_system_libwebp=false skia_use_system_expat=false skia_use_system_zlib=false skia_use_system_libpng=false skia_use_system_harfbuzz=false skia_use_system_icu=false skia_enable_gpu=true skia_use_gl=true"
		WORKING_DIRECTORY ${SKIA_DIR})
	add_library(skia STATIC IMPORTED)
	set_property(TARGET skia PROPERTY
		IMPORTED_LOCATION ${SKIA_BUILD_DIR}/libskia.a)
endif()
execute_process(COMMAND ninja WORKING_DIRECTORY ${SKIA_BUILD_DIR})

# LibUI for the renderer
set(LIBUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libui)
set(LIBUI_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/third_party/libui)
file(MAKE_DIRECTORY ${LIBUI_BUILD_DIR})
execute_process(
	COMMAND meson setup . ${LIBUI_DIR}
	WORKING_DIRECTORY ${LIBUI_BUILD_DIR})
execute_process(
	COMMAND meson configure -Ddefault_library=static
	WORKING_DIRECTORY ${LIBUI_BUILD_DIR})
execute_process( 
	COMMAND meson compile
	WORKING_DIRECTORY ${LIBUI_BUILD_DIR})
add_library(libui STATIC IMPORTED)
set_property(TARGET libui PROPERTY
	IMPORTED_LOCATION ${LIBUI_BUILD_DIR}/meson-out/libui.a)

# For OpenGL
find_package(glfw3 3.3 REQUIRED)

add_executable(streetview_client ${APPLICATION_TYPE}
	src/main.cpp
	src/parse.cpp
	src/extract.cpp
	src/renderer.cpp
	src/download.cpp
)

set_target_properties(streetview_client PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(streetview_client PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-braces)

set_target_properties(streetview_client PROPERTIES
		OUTPUT_NAME "streetview_client"
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		CXX_VISIBILITY_PRESET hidden
		POSITION_INDEPENDENT_CODE ON)

include_directories(streetview_client include fmt libcurl CLI11 glfw3 ${SKIA_DIR} ${SKIA_DIR}/include ${LIBUI_DIR} ${RAPIDJSON_INCLUDE_DIR})
target_link_libraries(streetview_client PUBLIC fmt libcurl CLI11 skia libui "-framework OpenGl" "-framework CoreFoundation" "-framework CoreGraphics" "-framework CoreText" "-framework CoreServices" "-framework Cocoa" "-framework Metal" "-framework Foundation" "-framework QuartzCore" glfw)