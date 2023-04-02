#include "headers.hpp"

curl_slist* get_main_page_headers() {
	curl_slist* main_page_headers = NULL;
	main_page_headers
		= curl_slist_append(main_page_headers, "Accept: "
											   "text/html,application/xhtml+xml,application/"
											   "xml;q=0.9,image/avif,image/webp,*/*;q=0.8");
	main_page_headers = curl_slist_append(main_page_headers, "Accept-Encoding: en-US,en;q=0.5");
	main_page_headers = curl_slist_append(main_page_headers, "Accept-Language: en-US,en;q=0.5");
	main_page_headers = curl_slist_append(main_page_headers, "Connection: keep-alive");
	main_page_headers = curl_slist_append(main_page_headers, "Host: www.google.com");
	main_page_headers = curl_slist_append(main_page_headers, "Sec-Fetch-Dest: document");
	main_page_headers = curl_slist_append(main_page_headers, "Sec-Fetch-Mode: navigate");
	main_page_headers = curl_slist_append(main_page_headers, "Sec-Fetch-Site: none");
	main_page_headers = curl_slist_append(main_page_headers, "Sec-Fetch-User: ?1");
	main_page_headers = curl_slist_append(main_page_headers, "TE: trailers");
	main_page_headers = curl_slist_append(main_page_headers, "Upgrade-Insecure-Requests: 1");
	main_page_headers = curl_slist_append(main_page_headers,
		"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; "
		"rv:109.0) Gecko/20100101 Firefox/111.0");
	return main_page_headers;
}

curl_slist* get_panorama_headers() {
	curl_slist* panorama_headers = NULL;
	panorama_headers
		= curl_slist_append(panorama_headers, "Accept: "
											  "text/html,application/xhtml+xml,application/"
											  "xml;q=0.9,image/avif,image/webp,*/*;q=0.8");
	panorama_headers = curl_slist_append(panorama_headers, "Accept-Encoding: en-US,en;q=0.5");
	panorama_headers = curl_slist_append(panorama_headers, "Accept-Language: en-US,en;q=0.5");
	panorama_headers
		= curl_slist_append(panorama_headers, "Alt-Used: streetviewpixels-pa.googleapis.com");
	panorama_headers = curl_slist_append(panorama_headers, "Connection: keep-alive");
	panorama_headers
		= curl_slist_append(panorama_headers, "Host: streetviewpixels-pa.googleapis.com");
	panorama_headers = curl_slist_append(panorama_headers, "Sec-Fetch-Dest: document");
	panorama_headers = curl_slist_append(panorama_headers, "Sec-Fetch-Mode: navigate");
	panorama_headers = curl_slist_append(panorama_headers, "Sec-Fetch-Site: none");
	panorama_headers = curl_slist_append(panorama_headers, "Sec-Fetch-User: ?1");
	panorama_headers = curl_slist_append(panorama_headers, "TE: trailers");
	panorama_headers = curl_slist_append(panorama_headers, "Upgrade-Insecure-Requests: 1");
	panorama_headers = curl_slist_append(panorama_headers,
		"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; "
		"rv:109.0) Gecko/20100101 Firefox/111.0");
	return panorama_headers;
}

curl_slist* get_photometa_headers() {
	curl_slist* photometa_headers = NULL;
	photometa_headers             = curl_slist_append(photometa_headers, "Accept: */*");
	photometa_headers = curl_slist_append(photometa_headers, "Accept-Encoding: en-US,en;q=0.5");
	photometa_headers = curl_slist_append(photometa_headers, "Accept-Language: en-US,en;q=0.5");
	photometa_headers = curl_slist_append(photometa_headers, "Connection: keep-alive");
	photometa_headers = curl_slist_append(photometa_headers, "Host: www.google.com");
	photometa_headers = curl_slist_append(photometa_headers, "Referer: https://www.google.com/");
	// TODO switch to cors
	photometa_headers = curl_slist_append(photometa_headers, "Sec-Fetch-Dest: document");
	photometa_headers = curl_slist_append(photometa_headers, "Sec-Fetch-Mode: navigate");
	photometa_headers = curl_slist_append(photometa_headers, "Sec-Fetch-Site: none");
	photometa_headers = curl_slist_append(photometa_headers, "Sec-Fetch-User: ?1");
	photometa_headers = curl_slist_append(photometa_headers, "TE: trailers");
	photometa_headers = curl_slist_append(photometa_headers,
		"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; "
		"rv:109.0) Gecko/20100101 Firefox/111.0");
	return photometa_headers;
}