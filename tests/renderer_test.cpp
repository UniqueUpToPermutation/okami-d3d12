#include <gtest/gtest.h>

#include "../engine.hpp"
#include "../renderer.hpp"
#include "../transform.hpp"
#include "../camera.hpp"
#include "../paths.hpp"
#include "../texture.hpp"

#include "utils.hpp"

#include <glog/logging.h>

using namespace okami;

bool CompareImages(std::filesystem::path const& a, std::filesystem::path const& b) {
	LOG(INFO) << "Comparing images: " << a << " and " << b;
	
	if (!std::filesystem::exists(a) || !std::filesystem::exists(b)) {
		return false; // One of the files does not exist
	}
	
	RawTexture textureA = *RawTexture::FromPNG(a);
	RawTexture textureB = *RawTexture::FromPNG(b);

	// Compare contents of the textures
	if (textureA.GetData().size() != textureB.GetData().size()) {
		return false;
	}

	return std::equal(textureA.GetData().begin(), textureA.GetData().end(),
		textureB.GetData().begin());
}

void CompareImages(Engine& engine) {
	auto path = engine.GetRenderOutputPath(0);
	EXPECT_TRUE(CompareImages(path,
		GetTestAssetPath(std::filesystem::path{"golden_images"} / path.filename())));
}

TEST(RendererTest, Triangle) {
	std::vector <const char*> argsv;
	Engine engine{ GetTestEngineParams(argsv, "render_triangle") };
#if defined(USE_D3D12)
#if defined(USE_D3D12)
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();
#else
	GTEST_SKIP() << "D3D12 not enabled; skipping renderer tests.";
#endif
#else
	GTEST_SKIP() << "D3D12 not enabled; skipping renderer tests.";
#endif

	if (auto err = engine.Startup(); err.IsError()) {
		FAIL() << "Engine startup failed: " << err;
		return;
	}

	// Render a triangle
	auto entity = engine.CreateEntity();
	engine.AddComponent(entity, DummyTriangleComponent{});

	// Render a single frame
	engine.Run(1);

	// Check correctness
	CompareImages(engine);
}

TEST(RendererTest, TwoTriangles) {
	std::vector <const char*> argsv;
	Engine engine{ GetTestEngineParams(argsv, "render_two_triangle") };
#if defined(USE_D3D12)
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();
#else
	GTEST_SKIP() << "D3D12 not enabled; skipping renderer tests.";
#endif

	if (auto err = engine.Startup(); err.IsError()) {
		FAIL() << "Engine startup failed: " << err;
		return;
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

	// Check correctness
	CompareImages(engine);
}

TEST(RendererTest, Cube) {
	std::vector <const char*> argsv;
	Engine engine{ GetTestEngineParams(argsv, "render_cube") };
#if defined(USE_D3D12)
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();
#else
	GTEST_SKIP() << "D3D12 not enabled; skipping renderer tests.";
#endif

    if (auto err = engine.Startup(); err.IsError()) {  
        FAIL() << "Engine startup failed: " << err;
		return;
    }

	auto meshLoader = engine.GetResourceManager<Geometry>();
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
	} while (!box.IsLoaded());

	// Render a single frame
    engine.Run(1);

	// Check correctness
	CompareImages(engine);
}

TEST(RendererTest, TwoMeshes) {
	std::vector <const char*> argsv;
	Engine engine{ GetTestEngineParams(argsv, "render_two_meshes") };
#if defined(USE_D3D12)
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();
#else
	GTEST_SKIP() << "D3D12 not enabled; skipping renderer tests.";
#endif

    if (auto err = engine.Startup(); err.IsError()) {  
        FAIL() << "Engine startup failed: " << err;
		return;
    }

	auto meshLoader = engine.GetResourceManager<Geometry>();
	auto box = meshLoader->Load(GetTestAssetPath("box.glb"));
	auto torus = meshLoader->Load(GetTestAssetPath("torus.glb"));

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
	engine.AddComponent(boxEntity, Transform::Translate(1.0f, 0.0f, 0.0f));

	auto torusEntity = engine.CreateEntity();
	engine.AddComponent(torusEntity, StaticMeshComponent{torus});
	engine.AddComponent(torusEntity, Transform::Translate(-1.0f, 0.0f, 0.0f));

	// Wait for the box mesh to be loaded,
	// otherwise the renderer will not be able to render it
	do {
		engine.UploadResources();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} while (!box.IsLoaded() || !torus.IsLoaded());

	// Render a single frame
    engine.Run(1);

	// Check correctness
	CompareImages(engine);
}

TEST(RendererTest, Sprites) {
	std::vector <const char*> argsv;
	Engine engine{ GetTestEngineParams(argsv, "render_sprite") };
#if defined(USE_D3D12)
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();
#else
	GTEST_SKIP() << "D3D12 not enabled; skipping renderer tests.";
#endif

    if (auto err = engine.Startup(); err.IsError()) {  
        FAIL() << "Engine startup failed: " << err;
		return;
    }

	auto texture = engine.Load<Texture>(GetTestAssetPath("test.png"));
    auto texture2 = engine.Load<Texture>(GetTestAssetPath("test2.png"));

    auto spriteEntity = engine.CreateEntity();
    engine.AddComponent(spriteEntity, SpriteComponent{ .m_texture = texture });
    engine.AddComponent(spriteEntity, Transform::_2D(-200.0f, 0.0f));
  
    auto spriteEntity2 = engine.CreateEntity();
    engine.AddComponent(spriteEntity2, SpriteComponent{ .m_texture = texture });
    engine.AddComponent(spriteEntity2, Transform::_2D(200.0f, 0.0f, glm::pi<float>() / 2.0f));

    auto spriteEntity3 = engine.CreateEntity();
    engine.AddComponent(spriteEntity3, SpriteComponent{ .m_texture = texture2, .m_color = color::Cyan });
    engine.AddComponent(spriteEntity3, Transform::_2D(0.0f, 0.0f, glm::pi<float>() / 4.0f));

    auto spriteEntity4 = engine.CreateEntity();
    engine.AddComponent(spriteEntity4, SpriteComponent{ .m_texture = texture, .m_color = color::Magenta });
    engine.AddComponent(spriteEntity4, Transform::_2D(-500.0f, 0.0f, glm::pi<float>() / 4.0f, 2.0f));

    auto spriteEntity5 = engine.CreateEntity();
    engine.AddComponent(spriteEntity5, SpriteComponent{ .m_texture = texture2, .m_color = color::Yellow });
    engine.AddComponent(spriteEntity5, Transform::_2D(500.0f, 0.0f, -glm::pi<float>() / 4.0f, 2.0f));

    auto spriteEntity6 = engine.CreateEntity();
    engine.AddComponent(spriteEntity6, SpriteComponent{ .m_texture = texture2, .m_sourceRect = Rect{ {0.f, 0.f}, {64.f, 64.f} } });
    engine.AddComponent(spriteEntity6, Transform::_2D(0.0f, 200.0f));

    auto spriteEntity7 = engine.CreateEntity();
    engine.AddComponent(spriteEntity7, SpriteComponent{ .m_texture = texture2, .m_sourceRect = Rect{ {32.f, 32.f}, {64.f, 64.f} } });
    engine.AddComponent(spriteEntity7, Transform::_2D(0.0f, -200.0f));

    auto cameraEntity = engine.CreateEntity();
    engine.AddComponent(cameraEntity, Camera::Orthographic(-1.0, 1.0));

	// Wait for the texture to be loaded
	// otherwise the renderer will not be able to render it
	do {
		engine.UploadResources();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} while (!texture.IsLoaded() || !texture2.IsLoaded());

    // Render a single frame
    engine.Run(1);

	// Check correctness
	CompareImages(engine);
}