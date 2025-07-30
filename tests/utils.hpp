#pragma once

#include <string_view>

#include <gtest/gtest.h>

#include "../engine.hpp"

okami::EngineParams GetTestEngineParams(std::vector<const char*>& argsv, std::string_view outputFileStem = "");
