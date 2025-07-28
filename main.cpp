#include "engine.hpp"

#include <windows.h>

using namespace okami;

int main(int argc, char const* argv[]) {
	Engine engine{EngineParams{
		.m_argc = argc,
		.m_argv = argv,
	}};
	engine.AddModuleFromFactory<D3D12RendererModuleFactory>();
	OKAMI_DEFER(engine.Shutdown());

	if (auto err = engine.Startup(); err.IsError()) {
		std::cerr << "Engine startup failed: " << err << std::endl;
	}

	engine.Run();

	return 0;
}
