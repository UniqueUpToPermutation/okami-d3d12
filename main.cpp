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

    engine.Run();

    return 0;  
}
