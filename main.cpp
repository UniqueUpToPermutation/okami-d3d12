#include <tiny_gltf.h>

#include "engine.hpp"
#include "camera.hpp"
#include "renderer.hpp"
#include "transform.hpp"
#include "paths.hpp"

using namespace okami;

#include <fstream>  

int main(int argc, char const* argv[]) {  
    Engine engine{ EngineParams{  
        .m_argc = argc,  
        .m_argv = argv,  
    } };  
    engine.AddModuleFromFactory<D3D12RendererModuleFactory>();  

    if (auto err = engine.Startup(); err.IsError()) {  
        std::cerr << "Engine startup failed: " << err << std::endl;  
    }

	auto meshLoader = engine.GetResourceManager<Mesh>();
	auto box = meshLoader->Load(GetAssetsPath() / "assets" / "box.glb");

    engine.Run();  

    return 0;  
}
