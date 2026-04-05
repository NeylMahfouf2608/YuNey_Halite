#pragma once

#include "log.hpp"

#include <string>
#include <iostream>
#include <sstream>

namespace hlt {
	static std::string get_string() {
		std::string result;
		std::getline(std::cin, result);
		if (!std::cin.good()) {
			exit(0);
		}
		return result;
	}

	static std::stringstream get_sstream() {
		return std::stringstream(get_string());
	}
}
