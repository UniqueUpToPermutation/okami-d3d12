#include "utils.hpp"

okami::EngineParams GetTestEngineParams(std::vector<const char*>& argsv, std::string_view outputFileStem) {
	auto args = ::testing::internal::GetArgvs();
	for (const auto& arg : args) {
		argsv.push_back(arg.c_str());
	}
	return okami::EngineParams{
		.m_argc = static_cast<int>(argsv.size()),
		.m_argv = argsv.data(),
		.m_headlessMode = true,
		.m_headlessOutputFileStem = outputFileStem
	};
}
