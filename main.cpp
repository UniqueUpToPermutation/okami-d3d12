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

	/*auto texture = engine.Load<Texture>(GetTestAssetPath("test.png"));
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
    engine.AddComponent(cameraEntity, Camera::Orthographic(-1.0, 1.0));*/

    engine.Run();

    return 0;  
}
