#include "engine.hpp"
#include "renderer.hpp"
#include "transform.hpp"

#include <windows.h>

using namespace okami;

int main(int argc, char const* argv[]) {
	Engine engine{EngineParams{
		.m_argc = argc,
		.m_argv = argv,
	}};
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();

	if (auto err = engine.Startup(); err.IsError()) {
		std::cerr << "Engine startup failed: " << err << std::endl;
	}

	auto& world = engine.GetEntityTree();
	auto& signalBus = engine.GetSignalBus();

	// Create the world
	auto triangle = world.CreateEntity(signalBus);
	signalBus.AddComponent(triangle, DummyTriangleComponent{});
	signalBus.AddComponent(triangle, Transform(glm::vec3(0.0f, 0.0f, 0.0f)));

	engine.Run();

	return 0;
}
