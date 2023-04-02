#pragma once

#include <curl/curl.h>

curl_slist* get_main_page_headers();
curl_slist* get_panorama_headers();
curl_slist* get_photometa_headers();