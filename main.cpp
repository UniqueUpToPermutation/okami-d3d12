#include "engine.hpp"

#include <windows.h>

using namespace okami;

int main(int argc, char const* argv[]) {
    Engine engine;
    engine.AddModuleFromFactory<D3D12RendererModuleFactory>();
    OKAMI_DEFER(engine.Shutdown());

    if (auto err = engine.Startup(argc, argv); err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
    }

    engine.Run();

    return 0;
}
