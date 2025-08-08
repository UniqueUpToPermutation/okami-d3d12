#include <gtest/gtest.h>

#include "../engine.hpp"
#include "../renderer.hpp"
#include "../transform.hpp"
#include "../camera.hpp"
#include "../paths.hpp"

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
	Engine engine{ GetTestEngineParams(argsv, "render_two_triangle") };
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

TEST(RendererTest, Cube) {
	std::vector <const char*> argsv;
	Engine engine{ GetTestEngineParams(argsv, "render_cube") };
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();

    if (auto err = engine.Startup(); err.IsError()) {  
        std::cerr << "Engine startup failed: " << err << std::endl;  
    }

	auto meshLoader = engine.GetResourceManager<Mesh>();
	auto box = meshLoader->Load(GetTestAssetPath("box.glb"));

    auto camera = engine.CreateEntity();
    engine.AddComponent(camera, 
        Camera::Perspective(
            glm::radians(90.0f), 0.1f, 10.0f));
    engine.AddComponent(camera, Transform::LookAt(
        glm::vec3(1.5f, 1.5f, 1.5f), 
        glm::vec3(0.0f, 0.0f, 0.0f), 
        glm::vec3(0.0f, 1.0f, 0.0f)));

    auto boxEntity = engine.CreateEntity();

    engine.AddComponent(boxEntity, StaticMeshComponent{box});

	// Wait for the box mesh to be loaded,
	// otherwise the renderer will not be able to render it
	do {
		engine.UploadResources();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} while (!box.IsValid());

	// Render a single frame
    engine.Run(1);
}