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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb,
                                    void *userp) {
  size_t realsize = size * nmemb;
  auto &mem = *static_cast<std::string *>(userp);
  mem.append(static_cast<char *>(contents), realsize);
  return realsize;
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
    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(*res)
              << std::endl;
  } else {
    std::cout << download.size() << " bytes retrieved" << std::endl;
  }

  return download;
}

int main(void) {
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
  auto start = main_page_download.find(find_str);
  if (start == std::string::npos) {
    std::cerr << "Start not found" << std::endl;
  }

  auto end_quote = main_page_download.find("\"", start + strlen(find_str));
  if (end_quote == std::string::npos) {
    std::cerr << "End quote not found" << std::endl;
  }

  auto id = std::string(main_page_download.begin() + start + strlen(find_str),
                        main_page_download.begin() + end_quote);
  std::cout << "Obtained id: " << id << std::endl;

  auto photo_preview_url = fmt::format(
      "https://www.google.com/maps/preview/"
      "photo?authuser=0&hl=en&gl=us&pb=!1e3!5m54!2m2!1i203!2i100!3m3!2i4!"
      "3s{}!5b1!7m42!1m3!1e1!2b0!3e3!1m3!1e2!2b1!3e2!1m3!1e2!2b0!"
      "3e3!1m3!1e8!2b0!3e3!1m3!1e10!2b0!3e3!1m3!1e10!2b1!3e2!1m3!1e9!2b1!3e2!"
      "1m3!1e10!2b0!3e3!1m3!1e10!2b1!3e2!1m3!1e10!2b0!3e4!2b1!4b1!8m0!9b0!11m1!"
      "4b1!6m3!1s{}!7e81!15i11021!9m2!2d{}!3d{}!10d{}",
      "CAEIBAgFCAYgAQ", id, 5.108114, 52.094514, 25);
  // Format is longitude, latitude, zoom. Zoom stays the same
  // Accuracy must be between 5 and 6 decimals

  auto photo_preview_download =
      download_from_url(photo_preview_url, curl_handle, &res, NULL);

  if (photo_preview_download.at(6) == 'n') {
    std::cerr << "No street view exists at this location" << std::endl;
    return 0;
  }

  end_quote = photo_preview_download.find("\"", 9);
  if (end_quote == std::string::npos) {
    std::cerr << "End quote not found" << std::endl;
  }
  auto tile_id = std::string(photo_preview_download.begin() + 9,
                             photo_preview_download.begin() + end_quote);

  std::cout << "Tile id: " << tile_id << std::endl;

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

  sk_sp<SkSurface> tile_surface =
      SkSurface::MakeRasterN32Premul(512 * 8, 512 * 4);

  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 8; x++) {
      auto tile_url =
          fmt::format("https://streetviewpixels-pa.googleapis.com/v1/"
                      "tile?cb_client=maps_sv.tactile&panoid={}&x={}&y={}&zoom="
                      "3&nbt=1&fover=2",
                      tile_id, x, y);
      auto tile_download =
          download_from_url(tile_url, curl_handle, &res, tile_headers);

      SkBitmap bitmap;
      std::unique_ptr<SkCodec> jpeg = SkCodec::MakeFromData(
          SkData::MakeWithCopy(tile_download.data(), tile_download.size()));
      if (!jpeg) {
        std::cerr << "Could not get from jpeg" << std::endl;
      }
      SkImageInfo info = jpeg->getInfo().makeColorType(kBGRA_8888_SkColorType);
      bitmap.allocPixels(info);
      jpeg->getPixels(info, bitmap.getPixels(), bitmap.rowBytes());
      bitmap.setImmutable();

      tile_surface->getCanvas()->drawImage(bitmap.asImage(), 512 * x, 512 * y);
    }
  }

  std::filesystem::create_directory("tiles");
  SkFILEWStream dest("tiles/stiched.png");
  SkPngEncoder::Options options;
  options.fZLibLevel = 9;

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(tile_surface->width(),
                                       tile_surface->height(),
                                       SkColorType::kRGB_888x_SkColorType,
                                       SkAlphaType::kOpaque_SkAlphaType),
                     0);

  tile_surface->getCanvas()->readPixels(bitmap, 0, 0);
  SkPixmap src;
  bitmap.peekPixels(&src);
  if (!SkPngEncoder::Encode(&dest, src, options)) {
    std::cout << "Could not render full image" << std::endl;
  }

  curl_easy_cleanup(curl_handle);
  curl_global_cleanup();
}