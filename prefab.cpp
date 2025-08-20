#include "prefab.hpp"

using namespace okami;

Error ScenePrefab::Spawn(entity_t e, 
    EntityTree& entityTree,
    ISignalBus& signalBus) const {

    auto maxEntity = std::max(
        m_entitiesToCreate.begin(),
        m_entitiesToCreate.end(),
        [](const EntityCreation& a, const EntityCreation& b) {
            return a.m_entity < b.m_entity;
        });

    std::vector<entity_t> entityMap;
    entityMap.resize(maxEntity->m_entity + 1, kNullEntity);

    for (auto toCreate : m_entitiesToCreate) {
        entityMap[toCreate.m_entity] = entityTree.CreateEntity(
            signalBus, entityMap[toCreate.m_parent]);
    }

    for (auto staticMesh : m_staticMeshesToCreate) {
        signalBus.AddComponent(
            entityMap[staticMesh.first],
            staticMesh.second);
    }
    
    return {};
}

ResHandle<ScenePrefab> PrefabManager::LoadGltf(std::string_view path) {
    
}

ResHandle<ScenePrefab> PrefabManager::Load(std::string_view path) {

}