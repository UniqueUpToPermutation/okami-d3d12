#include <gtest/gtest.h>

#include "../engine.hpp"
#include "../renderer.hpp"
#include "../transform.hpp"
#include "../camera.hpp"

#include "utils.hpp"

using namespace okami;

TEST(RendererTest, Triangle) {
	std::vector <const char*> argsv;
	Engine engine{ GetTestEngineParams(argsv, "render_triangle") };
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();

	if (auto err = engine.Startup(); err.IsError()) {
		GTEST_ASSERT_TRUE(false) << "Engine startup failed: " << err.Str();
	}

	// Render a triangle
	auto entity = engine.CreateEntity();
	engine.AddComponent(entity, DummyTriangleComponent{});

	// Render a single frame
	engine.Run(1);
}

TEST(RendererTest, TwoTriangles) {
	std::vector <const char*> argsv;
	Engine engine{ GetTestEngineParams(argsv, "render_two_triangles") };
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();

	if (auto err = engine.Startup(); err.IsError()) {
		GTEST_ASSERT_TRUE(false) << "Engine startup failed: " << err.Str();
	}

	// Create the world
	auto translate1 = Transform::Translate(-0.5f, 0.0f, 0.0f);
	auto translate2 = Transform::Translate(0.5f, 0.0f, 0.0f);

	auto triangle1 = engine.CreateEntity();
	engine.AddComponent(triangle1, DummyTriangleComponent{});
	engine.AddComponent(triangle1, translate1);

	auto triangle2 = engine.CreateEntity();
	engine.AddComponent(triangle2, DummyTriangleComponent{});
	engine.AddComponent(triangle2, translate2);

	auto camera = engine.CreateEntity();
	engine.AddComponent(camera, Camera{
		OrthographicProjection{
			.m_width = 3.0f,
			.m_nearZ = -1.0f,
			.m_farZ = 1.0f,
		}
		});

	// Render a single frame
	engine.Run(1);
}