#pragma once

#include <iostream>

#define CRITICAL_ERROR(x) do { \
 		std::cout << x << std::endl; \
		PostQuitMessage(1); \
		return; } while(false)