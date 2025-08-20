#pragma once

#include "engine.hpp"
#include "renderer.hpp"

namespace okami {
    class Prefab {
    public:
        virtual Error Spawn(entity_t e, 
			EntityTree& entityTree,
			ISignalBus& signalBus) const = 0;
    };

    class ScenePrefab : public Prefab {
    private:
        struct EntityCreation {
            entity_t m_entity;
            entity_t m_parent = kNullEntity;
        };

        std::vector<EntityCreation> m_entitiesToCreate;
        std::vector<std::pair<entity_t, StaticMeshComponent>> m_staticMeshesToCreate;

    public:
        Error Spawn(entity_t e, 
			EntityTree& entityTree,
			ISignalBus& signalBus) const override;	
    };

    class PrefabManager final : public IResourceManager<ScenePrefab> {
    private:
        std::unordered_map<std::string, 
            std::unique_ptr<Resource<ScenePrefab>>> m_resources;

        ResHandle<ScenePrefab> LoadGltf(std::string_view path);

    public:
        ResHandle<ScenePrefab> Load(std::string_view path) override;
    };
}