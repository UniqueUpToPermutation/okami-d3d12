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
	auto box = meshLoader->Load(GetAssetPath("box.glb"));

    auto camera = engine.CreateEntity();
    engine.AddComponent(camera, 
        Camera::Perspective(
            glm::radians(90.0f), 0.1f, 10.0f));
    engine.AddComponent(camera, Transform::LookAt(
        glm::vec3(3.0f, 3.0f, 3.0f), 
        glm::vec3(0.0f, 0.0f, 0.0f), 
        glm::vec3(0.0f, 1.0f, 0.0f)));

    auto boxEntity = engine.CreateEntity();

    engine.AddComponent(boxEntity, Transform::Identity());
    engine.AddComponent(boxEntity, StaticMeshComponent{box});

    engine.AddScript([&](
        Time const& time,
		ISignalBus& signalBus,
		EntityTree& entityTree) {
    
        signalBus.UpdateComponent(boxEntity, 
            Transform::RotateY(static_cast<float>(time.m_totalTime * glm::radians(90.0f))));

    }, "Rotate Object");

    engine.Run();

    return 0;  
}
