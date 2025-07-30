#include <gtest/gtest.h>

#include "../engine.hpp"

#include "utils.hpp"

using namespace okami;

TEST(RendererTest, CreateAndRender) {
	std::vector <const char*> argsv;
	Engine engine{GetTestEngineParams(argsv, "create_and_render")};
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();

	if (auto err = engine.Startup(); err.IsError()) {
		GTEST_ASSERT_TRUE(false) << "Engine startup failed: " << err.Str();
	}

	// Render a single frame
	engine.Run(1);
}