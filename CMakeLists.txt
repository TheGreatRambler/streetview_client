cmake_minimum_required(VERSION 3.10)
set(STREETVIEW_CLIENT_VERSION "0.0.1")
project(streetview_client VERSION ${STREETVIEW_CLIENT_VERSION})

set(CMAKE_CXX_STANDARD 20)

add_compile_options(-Wno-sign-compare)
add_compile_options(-Wno-deprecated-declarations)
add_compile_options(-Wno-unused-parameter)
add_compile_options(-Wno-extern-initializer)
add_compile_options(-Wno-deprecated-enum-enum-conversion)

if(CMAKE_BUILD_TYPE EQUAL "DEBUG")
	add_link_options(-g -O0)
endif()

# fmt for some formatting
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt ${CMAKE_CURRENT_BINARY_DIR}/third_party/fmt)

# Libcurl for downloading files
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/curl ${CMAKE_CURRENT_BINARY_DIR}/third_party/curl)

# CLI11 for better command line support
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/CLI11 ${CMAKE_CURRENT_BINARY_DIR}/third_party/CLI11)

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
		COMMAND bin/gn gen ${SKIA_BUILD_DIR} "--args=is_official_build=true skia_use_system_libjpeg_turbo=false skia_use_system_libwebp=false skia_use_system_expat=false skia_use_system_zlib=false skia_use_system_libpng=false skia_use_system_harfbuzz=false skia_use_system_icu=false"
		WORKING_DIRECTORY ${SKIA_DIR})
	add_library(skia STATIC IMPORTED)
	set_property(TARGET skia PROPERTY
		IMPORTED_LOCATION ${SKIA_BUILD_DIR}/libskia.a)
endif()
execute_process(COMMAND ninja WORKING_DIRECTORY ${SKIA_BUILD_DIR})

add_executable(streetview_client ${APPLICATION_TYPE}
	src/main.cpp
)

set_target_properties(streetview_client PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(streetview_client PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-braces)

set_target_properties(streetview_client PROPERTIES
		OUTPUT_NAME "streetview_client"
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		CXX_VISIBILITY_PRESET hidden
		POSITION_INDEPENDENT_CODE ON)

include_directories(streetview_client include fmt libcurl CLI11::CLI11 ${SKIA_DIR} ${SKIA_DIR}/include)
target_link_libraries(streetview_client PUBLIC fmt libcurl CLI11::CLI11 skia "-framework CoreFoundation" "-framework CoreGraphics" "-framework CoreText" "-framework CoreServices")