#include <gtest/gtest.h>
#include "../engine.hpp"

using namespace okami;

TEST(RendererTest, CreateAndRender) {
	auto args = ::testing::internal::GetArgvs();
	std::vector<const char*> argsv;
	for (const auto& arg : args) {
		argsv.push_back(arg.c_str());
	}

	Engine engine{ EngineParams{
		.m_argc = static_cast<int>(argsv.size()),
		.m_argv = argsv.data(),
		.m_headlessMode = true,
		.m_headlessOutputFileStem = "renderer_test_output"
	} };
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();
	OKAMI_DEFER(engine.Shutdown());

	if (auto err = engine.Startup(); err.IsError()) {
		GTEST_ASSERT_TRUE(false) << "Engine startup failed: " << err.Str();
	}

	// Render a single frame
	engine.Run();
}