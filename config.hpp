#pragma once

#include "engine.hpp"

namespace okami {
	class IConfigModule {
	public:
		virtual std::optional<int> GetInt(std::string_view key) const = 0;
		virtual std::optional<float> GetFloat(std::string_view key) const = 0;
		virtual std::optional<std::string> GetString(std::string_view key) const = 0;
		virtual ~IConfigModule() = default;
	};
}