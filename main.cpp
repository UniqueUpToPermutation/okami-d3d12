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
	auto box = meshLoader->Load(GetAssetPath("duck.glb"));

    auto camera = engine.CreateEntity();
    engine.AddComponent(camera, 
        Camera::Orthographic(200.0f, -100.0f, 100.0f));
    engine.AddComponent(camera, 
        Transform::Translate(0.0f, 50.0f, 5.0f));

    auto boxEntity = engine.CreateEntity();
    engine.AddComponent(boxEntity, Transform::Identity());
    engine.AddComponent(boxEntity, StaticMeshComponent{box});

    engine.AddScript([boxEntity](
        Time const& time,
		ISignalBus& signalBus,
		EntityTree& entityTree) {

        signalBus.UpdateComponent<Transform>(boxEntity,
            Transform::RotateY(static_cast<float>(time.m_totalTime) * 0.5f));
       
    }, "Rotate Object");

    engine.Run();

    return 0;  
}
