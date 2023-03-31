#include "parse.hpp"

std::string parse_client_id(std::string main_page) {
	auto find_str = "\"],null,0,\"";
	auto start_id = main_page.find(find_str);
	if(start_id == std::string::npos) {
		return "";
	}

	auto end_quote = main_page.find("\"", start_id + strlen(find_str));
	if(end_quote == std::string::npos) {
		return "";
	}

	auto id = std::string(
		main_page.begin() + start_id + strlen(find_str), main_page.begin() + end_quote);
	return id;
}