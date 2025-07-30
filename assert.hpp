#pragma once

#include <stdexcept>

#ifndef NDEBUG 
#define OKAMI_ASSERT(condition, message) \
	do { \
		if (!(condition)) { \
			throw std::runtime_error(message); \
		} \
	} while (false) 
#else
#define OKAMI_ASSERT(condition, message) 
#endif