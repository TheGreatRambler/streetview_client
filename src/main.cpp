#define RAPIDJSON_HAS_STDSTRING 1

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <codec/SkCodec.h>
#include <core/SkBitmap.h>
#include <core/SkCanvas.h>
#include <core/SkColor.h>
#include <core/SkFont.h>
#include <core/SkGraphics.h>
#include <core/SkImage.h>
#include <core/SkStream.h>
#include <core/SkSurface.h>
#include <core/SkTextBlob.h>
#include <core/SkTypeface.h>
#include <curl/curl.h>
#include <encode/SkPngEncoder.h>
#include <fmt/format.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb,
                                    void *userp) {
  size_t realsize = size * nmemb;
  auto &mem = *static_cast<std::string *>(userp);
  mem.append(static_cast<char *>(contents), realsize);
  return realsize;
}

constexpr int SCREEN_PIXEL_HEIGHT = 768;
constexpr double PI = 3.14159;
constexpr double MinLatitude = -85.05112878;
constexpr double MaxLatitude = 85.05112878;
constexpr double MinLongitude = -180;
constexpr double MaxLongitude = 180;

double altitude_from_zoom_and_latitude(double zoom, double latitude) {
  constexpr int EARTH_RADIUS_IN_METERS = 6371010;
  constexpr int TILE_SIZE = 256;
  constexpr double RADIUS_X_PIXEL_HEIGHT =
      27.3611 * EARTH_RADIUS_IN_METERS * SCREEN_PIXEL_HEIGHT;
  return (RADIUS_X_PIXEL_HEIGHT * cos((latitude * PI) / 180)) /
         (pow(2, zoom) * TILE_SIZE);
}

double clip_number(double n, double minValue, double maxValue) {
  return std::min(std::max(n, minValue), maxValue);
}

std::pair<int, int> lat_long_to_x_y(double latitude, double longitude,
                                    int zoom) {
  latitude = clip_number(latitude, MinLatitude, MaxLatitude);
  longitude = clip_number(longitude, MinLongitude, MaxLongitude);

  double x = (longitude + 180) / 360;
  double sinLatitude = sin(latitude * PI / 180);
  double y = 0.5 - log((1 + sinLatitude) / (1 - sinLatitude)) / (4 * PI);

  uint mapSize = 2 << (zoom - 1);
  return std::make_pair((int)clip_number(x * mapSize + 0.5, 0, mapSize - 1),
                        (int)clip_number(y * mapSize + 0.5, 0, mapSize - 1));
}

std::string download_from_url(std::string url, CURL *curl_handle, CURLcode *res,
                              curl_slist *headers) {
  std::string download;
  curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &download);
  if (headers) {
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
  }

  // added options that may be required
  curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L); // redirects
  curl_easy_setopt(curl_handle, CURLOPT_HTTPPROXYTUNNEL,
                   1L); // corp. proxies etc.
  // curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L); // we want it all
  //  curl_easy_setopt(curl_handle, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP |
  //  CURLPROTO_HTTPS);

  *res = curl_easy_perform(curl_handle);

  if (*res != CURLE_OK) {
    std::cerr << "Downloading failed: " << curl_easy_strerror(*res)
              << std::endl;
  }

  return download;
}

int main(int argc, char **argv) {
  CLI::App app{"Street View custom client in C++"};
  double lat;
  app.add_option("--lat", lat, "Latitude")->required();
  double lng;
  app.add_option("--long", lng, "Longitude")->required();
  int range = 10000;
  app.add_option("-r,--range", range, "Range from location");
  int num_previews = 100;
  app.add_option("-n,--num-previews", num_previews, "Number of previews");
  int streetview_zoom = 2;
  app.add_option("-z,--zoom", streetview_zoom,
                 "How much to zoom in street view images, higher numbers "
                 "increase resolution");

  CLI11_PARSE(app, argc, argv);
  std::chrono::time_point<std::chrono::steady_clock> start;
  std::chrono::time_point<std::chrono::steady_clock> stop;

  start = std::chrono::high_resolution_clock::now();

  // auto lat = 39.754555;
  // auto lng = -105.221849;
  // auto range = 100000;
  // auto num_previews = 100;
  // auto streetview_zoom = 1;
  //  auto z = altitude_from_zoom_and_latitude(zoom, lat);
  //  std::cout << z << std::endl;

  curl_global_init(CURL_GLOBAL_ALL);
  auto curl_handle = curl_easy_init();

  curl_slist *main_page_headers = NULL;
  main_page_headers = curl_slist_append(
      main_page_headers, "Accept: "
                         "text/html,application/xhtml+xml,application/"
                         "xml;q=0.9,image/avif,image/webp,*/*;q=0.8");
  main_page_headers =
      curl_slist_append(main_page_headers, "Accept-Encoding: en-US,en;q=0.5");
  main_page_headers =
      curl_slist_append(main_page_headers, "Accept-Language: en-US,en;q=0.5");
  main_page_headers =
      curl_slist_append(main_page_headers, "Connection: keep-alive");
  main_page_headers =
      curl_slist_append(main_page_headers, "Host: www.google.com");
  main_page_headers =
      curl_slist_append(main_page_headers, "Sec-Fetch-Dest: document");
  main_page_headers =
      curl_slist_append(main_page_headers, "Sec-Fetch-Mode: navigate");
  main_page_headers =
      curl_slist_append(main_page_headers, "Sec-Fetch-Site: none");
  main_page_headers =
      curl_slist_append(main_page_headers, "Sec-Fetch-User: ?1");
  main_page_headers = curl_slist_append(main_page_headers, "TE: trailers");
  main_page_headers =
      curl_slist_append(main_page_headers, "Upgrade-Insecure-Requests: 1");
  main_page_headers = curl_slist_append(
      main_page_headers,
      "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; "
      "rv:109.0) Gecko/20100101 Firefox/111.0");
  CURLcode res;
  auto main_page_download = download_from_url(
      "https://www.google.com/maps", curl_handle, &res, main_page_headers);

  auto find_str = "\"],null,0,\"";
  auto start_id = main_page_download.find(find_str);
  if (start_id == std::string::npos) {
    std::cerr << "Start not found" << std::endl;
  }

  auto end_quote = main_page_download.find("\"", start_id + strlen(find_str));
  if (end_quote == std::string::npos) {
    std::cerr << "End quote not found" << std::endl;
  }

  auto id =
      std::string(main_page_download.begin() + start_id + strlen(find_str),
                  main_page_download.begin() + end_quote);
  std::cout << "Client id: " << id << std::endl;

  /*
    // Get x y coordinates
    auto x_y = lat_long_to_x_y(lat, lng, zoom);
    std::cout << x_y.first << " " << x_y.second << " " << zoom << std::endl;

    int x = x_y.first;
    int y = x_y.second;
    int unk1 = 4;
    int flag = 65535;
    auto streetview_lines_url = fmt::format(
        "https://www.google.com/maps/vt/stream/"
        "pb=!1m7!8m6!1m3!1i{}!2i{}!3i{}!2i{}!3x{}!2m3!1e0!2sm!3i640338396!2m2!"
        "1e2!2sspotlit!2m8!1e2!2ssvv!4m2!1scc!2s*211m3*211e2*212b1*213e2*211m3*"
        "211e10*212b1*213e2*211m3*211e9*212b1*213e2*211m3*211e10*212b1*213e2*"
        "212b1*214b1!4m2!1ssvl!2s*211b1*212b1!3m8!2sen!3sus!5e1105!12m4!1e68!2m2!"
        "1sset!2sRoadmap!4e1!5m4!1e4!8m2!1e0!1e1!6m16!1e12!2i2!11e2!19m1!1e0!"
        "20m2!1e1!2e0!30m1!1f2!39b1!44e1!50e0!67m1!1e1!71b1!23i10203575!"
        "23i1381033!23i1368782!23i1368785!23i47025228!23i4592408!23i4640515!"
        "23i4897086!23i1375050!23i4536287!23i47054629!27m14!299174093m13!14m12!"
        "1m8!1m2!1y0!2y3072202684587203839!2s%2Ffake_latlng_mid!4m2!1x520986111!"
        "2x51265278!8b1!2b0!3b0!4b0!28i640&authuser=0",
        zoom, x, y, unk1, flag);
    std::cout << streetview_lines_url << std::endl;
    auto streetview_lines =
        download_from_url(streetview_lines_url, curl_handle, &res, NULL);

    if (streetview_lines.at(6) == 0) {
      // There are no lines
    } else {
    }
    */

  // https://www.google.com/maps/preview/photo?authuser=0&hl=en&gl=us&pb=!1e3!5m54!2m2!1i203!2i100!3m3!2i4!3sCAEIBAgFCAYgAQ!5b1!7m42!1m3!1e1!2b0!3e3!1m3!1e2!2b1!3e2!1m3!1e2!2b0!3e3!1m3!1e8!2b0!3e3!1m3!1e10!2b0!3e3!1m3!1e10!2b1!3e2!1m3!1e9!2b1!3e2!1m3!1e10!2b0!3e3!1m3!1e10!2b1!3e2!1m3!1e10!2b0!3e4!2b1!4b1!8m0!9b0!11m1!4b1!6m3!1sChQjZNPeL7aH0PEP4eKbkAE!7e81!15i11021!9m2!2d5.139826079358205!3d47.8029739538609!10d2965.159912287723

  auto photo_preview_url = fmt::format(
      "https://www.google.com/maps/preview/"
      "photo?authuser=0&hl=en&gl=us&pb=!1e3!5m54!2m2!1i203!2i100!3m3!2i{}!"
      "3s{}!5b1!7m42!1m3!1e1!2b0!3e3!1m3!1e2!2b1!3e2!1m3!1e2!2b0!3e3!1m3!1e8!"
      "2b0!3e3!1m3!1e10!2b0!3e3!1m3!1e10!2b1!3e2!1m3!1e9!2b1!3e2!1m3!1e10!2b0!"
      "3e3!1m3!1e10!2b1!3e2!1m3!1e10!2b0!3e4!2b1!4b1!8m0!9b0!11m1!4b1!6m3!1s{}"
      "!"
      "7e81!15i11021!9m2!2d{}!3d{}!10d{}",
      num_previews, "CAEIBAgFCAYgAQ", id, lng, lat, range);
  // Format is longitude, latitude, zoom. Zoom stays the same
  // Accuracy must be between 5 and 6 decimals

  auto photo_preview_download =
      download_from_url(photo_preview_url, curl_handle, &res, NULL);

  // Parse into JSON
  rapidjson::Document preview_document;
  preview_document.Parse(photo_preview_download.substr(4));

  // std::cout << photo_preview_url << std::endl;

  curl_slist *tile_headers = NULL;
  tile_headers = curl_slist_append(
      tile_headers, "Accept: "
                    "text/html,application/xhtml+xml,application/"
                    "xml;q=0.9,image/avif,image/webp,*/*;q=0.8");
  tile_headers =
      curl_slist_append(tile_headers, "Accept-Encoding: en-US,en;q=0.5");
  tile_headers =
      curl_slist_append(tile_headers, "Accept-Language: en-US,en;q=0.5");
  tile_headers = curl_slist_append(
      tile_headers, "Alt-Used: streetviewpixels-pa.googleapis.com");
  tile_headers = curl_slist_append(tile_headers, "Connection: keep-alive");
  tile_headers = curl_slist_append(tile_headers,
                                   "Host: streetviewpixels-pa.googleapis.com");
  tile_headers = curl_slist_append(tile_headers, "Sec-Fetch-Dest: document");
  tile_headers = curl_slist_append(tile_headers, "Sec-Fetch-Mode: navigate");
  tile_headers = curl_slist_append(tile_headers, "Sec-Fetch-Site: none");
  tile_headers = curl_slist_append(tile_headers, "Sec-Fetch-User: ?1");
  tile_headers = curl_slist_append(tile_headers, "TE: trailers");
  tile_headers =
      curl_slist_append(tile_headers, "Upgrade-Insecure-Requests: 1");
  tile_headers = curl_slist_append(
      tile_headers, "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; "
                    "rv:109.0) Gecko/20100101 Firefox/111.0");

  curl_slist *photometa_headers = NULL;
  photometa_headers = curl_slist_append(photometa_headers, "Accept: */*");
  photometa_headers =
      curl_slist_append(photometa_headers, "Accept-Encoding: en-US,en;q=0.5");
  photometa_headers =
      curl_slist_append(photometa_headers, "Accept-Language: en-US,en;q=0.5");
  photometa_headers =
      curl_slist_append(photometa_headers, "Connection: keep-alive");
  photometa_headers =
      curl_slist_append(photometa_headers, "Host: www.google.com");
  photometa_headers =
      curl_slist_append(photometa_headers, "Referer: https://www.google.com/");
  // TODO switch to cors
  photometa_headers =
      curl_slist_append(photometa_headers, "Sec-Fetch-Dest: document");
  photometa_headers =
      curl_slist_append(photometa_headers, "Sec-Fetch-Mode: navigate");
  photometa_headers =
      curl_slist_append(photometa_headers, "Sec-Fetch-Site: none");
  photometa_headers =
      curl_slist_append(photometa_headers, "Sec-Fetch-User: ?1");
  photometa_headers = curl_slist_append(photometa_headers, "TE: trailers");
  photometa_headers = curl_slist_append(
      photometa_headers,
      "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; "
      "rv:109.0) Gecko/20100101 Firefox/111.0");

  stop = std::chrono::high_resolution_clock::now();
  fmt::print("Setup took {}ms\n",
             std::chrono::duration_cast<std::chrono::milliseconds>(stop - start)
                 .count());

  if (preview_document.IsArray() && preview_document.Size() == 11) {
    if (preview_document[0].IsArray() && preview_document[0].Size() > 0) {
      std::cout << preview_document[0].Size() << " previews" << std::endl;
      for (auto &preview : preview_document[0].GetArray()) {
        if (preview.IsArray() && preview.Size() == 32) {
          if (preview[0].IsString()) {
            start = std::chrono::high_resolution_clock::now();
            auto tile_id = std::string(preview[0].GetString(),
                                       preview[0].GetStringLength());
            // std::cout << "Tile id: " << tile_id << std::endl;

            if (tile_id.size() != 22) {
              // std::cout << fmt::format("{} is not a street view", tile_id)
              //           << std::endl;
              continue;
            }

            // Get photometa
            auto photometa_url = fmt::format(
                "https://www.google.com/maps/photometa/"
                "v1?authuser=0&hl=en&gl=us&pb=!1m4!1smaps_sv.tactile!11m2!2m1!"
                "1b1!2m2!1sen!2sus!3m3!1m2!1e2!2s{}!4m57!1e1!1e2!1e3!1e4!1e5!"
                "1e6!1e8!1e12!2m1!1e1!4m1!1i48!5m1!1e1!5m1!1e2!6m1!1e1!6m1!1e2!"
                "9m36!1m3!1e2!2b1!3e2!1m3!1e2!2b0!3e3!1m3!1e3!2b1!3e2!1m3!1e3!"
                "2b0!3e3!1m3!1e8!2b0!3e3!1m3!1e1!2b0!3e3!1m3!1e4!2b0!3e3!1m3!"
                "1e10!2b1!3e2!1m3!1e10!2b0!3e3",
                tile_id);
            auto photometa_download = download_from_url(
                photometa_url, curl_handle, &res, photometa_headers);
            rapidjson::Document photometa_document;
            photometa_document.Parse(photometa_download.substr(4));

            std::string filename;
            auto &outer_location = photometa_document[1][0][3];
            if (outer_location.IsArray() && outer_location.Size() > 2 &&
                outer_location[2].IsArray()) {
              if (outer_location[2].Size() == 1) {
                // No address, just city
                filename = fmt::format("tiles/stitched-{}-{}.png", tile_id,
                                       outer_location[2][0].GetString());
              } else if (outer_location[2].Size() == 2) {
                // Address and city
                filename = fmt::format("tiles/stitched-{}-{}, {}.png", tile_id,
                                       outer_location[2][0].GetString(),
                                       outer_location[2][1].GetString());
              }
            } else {
              filename = fmt::format("tiles/stitched-{}.png", tile_id);
            }

            auto &outer_adjacent = photometa_document[1][0][5];
            if (outer_adjacent.IsArray() && outer_adjacent.Size() == 1 &&
                outer_adjacent[0].IsArray() && outer_adjacent[0].Size() == 13 &&
                outer_adjacent[0][3].IsArray() &&
                outer_adjacent[0][3].Size() == 1 &&
                outer_adjacent[0][3][0].IsArray()) {
              // Confirmed the list exists
              for (auto &adjacent : outer_adjacent[0][3][0].GetArray()) {
                if (adjacent.IsArray() && adjacent.Size() == 3 &&
                    adjacent[0].IsArray() && adjacent[0].Size() == 2) {
                  auto adjacent_tile = adjacent[0][1].GetString();
                  std::cout << "Adjacent: " << adjacent_tile << std::endl;
                }
              }
            }

            auto &tiles_dimensions = photometa_document[1][0][2][2];
            double tiles_height = tiles_dimensions[0].GetInt() / 512 /
                                  pow(2, 5 - streetview_zoom);
            double tiles_width = tiles_dimensions[1].GetInt() / 512 /
                                 pow(2, 5 - streetview_zoom);
            // std::cout << "Using width " << tiles_width << " and height "
            //           << tiles_height << std::endl;

            sk_sp<SkSurface> tile_surface = SkSurface::MakeRasterN32Premul(
                tiles_width * 512, tiles_height * 512);

            // Start processing
            tile_surface->getCanvas()->clear(SK_ColorWHITE);
            for (int y = 0; y < tiles_height; y++) {
              for (int x = 0; x < tiles_width; x++) {
                auto tile_url =
                    fmt::format("https://streetviewpixels-pa.googleapis.com/v1/"
                                "tile?cb_client=maps_sv.tactile&panoid={}&x={}&"
                                "y={}&zoom={}&nbt=1&fover=2",
                                tile_id, x, y, streetview_zoom);
                auto tile_download = download_from_url(tile_url, curl_handle,
                                                       &res, tile_headers);

                if (res == CURLE_OK) {
                  long http_code = 0;
                  curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE,
                                    &http_code);
                  if (http_code == 200) {
                    SkBitmap bitmap;
                    std::unique_ptr<SkCodec> jpeg =
                        SkCodec::MakeFromData(SkData::MakeWithCopy(
                            tile_download.data(), tile_download.size()));
                    if (!jpeg) {
                      std::cerr << "Could not get image from jpeg data: "
                                << tile_download << std::endl;
                    }
                    SkImageInfo info =
                        jpeg->getInfo().makeColorType(kBGRA_8888_SkColorType);
                    bitmap.allocPixels(info);
                    jpeg->getPixels(info, bitmap.getPixels(),
                                    bitmap.rowBytes());
                    bitmap.setImmutable();

                    tile_surface->getCanvas()->drawImage(bitmap.asImage(),
                                                         512 * x, 512 * y);
                  } else {
                    std::cout << "Error code " << http_code << std::endl;
                  }
                }
              }
            }

            std::filesystem::create_directory("tiles");
            SkFILEWStream dest(filename.c_str());
            SkPngEncoder::Options options;
            options.fZLibLevel = 9;

            SkBitmap bitmap;
            bitmap.allocPixels(
                SkImageInfo::Make(tile_surface->width(), tile_surface->height(),
                                  SkColorType::kRGB_888x_SkColorType,
                                  SkAlphaType::kOpaque_SkAlphaType),
                0);

            tile_surface->getCanvas()->readPixels(bitmap, 0, 0);
            SkPixmap src;
            bitmap.peekPixels(&src);
            if (!SkPngEncoder::Encode(&dest, src, options)) {
              std::cout << fmt::format("Could not render {}", tile_id)
                        << std::endl;
            }

            stop = std::chrono::high_resolution_clock::now();
            fmt::print("Downloading {} took {}ms\n", tile_id,
                       std::chrono::duration_cast<std::chrono::milliseconds>(
                           stop - start)
                           .count());
          } else {
            std::cout << "Preview id is not a string" << std::endl;
          }
        } else {
          std::cout << "Preview is not well formed" << std::endl;
        }
      }
    } else {
      std::cout << "No street view previews here" << std::endl;
    }
  } else {
    std::cout << "Is not well formed array" << std::endl;
  }

  curl_easy_cleanup(curl_handle);
  curl_global_cleanup();
}