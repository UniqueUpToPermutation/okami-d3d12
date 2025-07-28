#include "world.hpp"
#include <unordered_map>
#include <iostream>

using namespace okami;

// Define WorldNode and WorldImpl in the .cpp file to keep them private
struct WorldNode {
	entity_t m_entityId = kNullEntity;
	entity_t m_parent = kNullEntity;
	entity_t m_firstChild = kNullEntity;
	entity_t m_lastChild = kNullEntity;
	entity_t m_nextSibling = kNullEntity;
	entity_t m_previousSibling = kNullEntity;

	WorldNode(entity_t id) : m_entityId(id) {}
	WorldNode() = default; // Default constructor for empty nodes
};

namespace okami {
	struct WorldImpl {
		entity_t m_nextEntityId = 1;
		std::unordered_map<entity_t, WorldNode> m_entities;

		WorldImpl() {
			m_entities.emplace(kRoot, WorldNode(kRoot));
			m_entities[kRoot].m_parent = kNullEntity;
		}

		entity_t GetNextSibling(entity_t entity) const {
			auto it = m_entities.find(entity);
			return (it != m_entities.end()) ? it->second.m_nextSibling : kNullEntity;
		}

		entity_t GetFirstChild(entity_t entity) const {
			auto it = m_entities.find(entity);
			return (it != m_entities.end()) ? it->second.m_firstChild : kNullEntity;
		}

		entity_t GetParent(entity_t entity) const {
			auto it = m_entities.find(entity);
			return (it != m_entities.end()) ? it->second.m_parent : kNullEntity;
		}
	};
}

// Constructor
World::World(ISignalBus* bus) :
	m_impl(std::make_unique<WorldImpl>()),
	m_signalBus(bus) {
}

World::World(Engine * engine) :
	World(engine->GetSignalBus()) {
}

// Destructor - must be defined after WorldImpl is complete
World::~World() = default;

// Move operations - must be defined after WorldImpl is complete
World::World(World&&) noexcept = default;
World& World::operator=(World&&) noexcept = default;

entity_t World::CreateEntity(entity_t parent) {
	entity_t newId = m_impl->m_nextEntityId++;
	AddEntity(newId, parent);
	return newId;
}

void World::AddEntity(entity_t entity, entity_t parent) {
	if (m_impl->m_entities.count(entity)) {
		return; // Already exists
	}

	auto parentIt = m_impl->m_entities.find(parent);
	if (parentIt == m_impl->m_entities.end()) {
		return; // Parent doesn't exist
	}

	WorldNode newNode(entity);
	newNode.m_parent = parent;

	auto& parentNode = parentIt->second;
	if (parentNode.m_firstChild == kNullEntity) {
		parentNode.m_firstChild = entity;
		parentNode.m_lastChild = entity;
	}
	else {
		if (auto lastChildIt = m_impl->m_entities.find(parentNode.m_lastChild);
			lastChildIt != m_impl->m_entities.end()) {
			lastChildIt->second.m_nextSibling = entity;
			newNode.m_previousSibling = parentNode.m_lastChild;
		}
		parentNode.m_lastChild = entity;
	}

	m_impl->m_entities.emplace(entity, std::move(newNode));

	if (entity >= m_impl->m_nextEntityId) {
		m_impl->m_nextEntityId = entity + 1;
	}
}

void World::RemoveEntity(entity_t entity) {
	if (entity == kRoot) return;

	auto entityIt = m_impl->m_entities.find(entity);
	if (entityIt == m_impl->m_entities.end()) return;

	const auto& node = entityIt->second;

	// Remove children recursively
	entity_t child = node.m_firstChild;
	while (child != kNullEntity) {
		if (auto childIt = m_impl->m_entities.find(child);
			childIt != m_impl->m_entities.end()) {
			entity_t nextChild = childIt->second.m_nextSibling;
			RemoveEntity(child);
			child = nextChild;
		}
		else {
			break;
		}
	}

	// Update parent's child pointers
	if (node.m_parent != kNullEntity) {
		if (auto parentIt = m_impl->m_entities.find(node.m_parent);
			parentIt != m_impl->m_entities.end()) {
			auto& parent = parentIt->second;
			if (parent.m_firstChild == entity) {
				parent.m_firstChild = node.m_nextSibling;
			}
			if (parent.m_lastChild == entity) {
				parent.m_lastChild = node.m_previousSibling;
			}
		}
	}

	// Update sibling links
	if (node.m_previousSibling != kNullEntity) {
		if (auto prevIt = m_impl->m_entities.find(node.m_previousSibling);
			prevIt != m_impl->m_entities.end()) {
			prevIt->second.m_nextSibling = node.m_nextSibling;
		}
	}
	if (node.m_nextSibling != kNullEntity) {
		if (auto nextIt = m_impl->m_entities.find(node.m_nextSibling);
			nextIt != m_impl->m_entities.end()) {
			nextIt->second.m_previousSibling = node.m_previousSibling;
		}
	}

	m_impl->m_entities.erase(entityIt);
}

void World::SetParent(entity_t entity, entity_t newParent) {
	if (entity == kRoot) return;

	auto entityIt = m_impl->m_entities.find(entity);
	auto parentIt = m_impl->m_entities.find(newParent);

	if (entityIt == m_impl->m_entities.end() ||
		parentIt == m_impl->m_entities.end()) {
		return;
	}

	// Check for circular dependency
	entity_t ancestor = newParent;
	while (ancestor != kNullEntity) {
		if (ancestor == entity) return;
		if (auto ancestorIt = m_impl->m_entities.find(ancestor);
			ancestorIt != m_impl->m_entities.end()) {
			ancestor = ancestorIt->second.m_parent;
		}
		else {
			break;
		}
	}

	auto& node = entityIt->second;
	entity_t oldParent = node.m_parent;

	// Remove from old parent
	if (oldParent != kNullEntity) {
		if (auto oldParentIt = m_impl->m_entities.find(oldParent);
			oldParentIt != m_impl->m_entities.end()) {
			auto& oldParentNode = oldParentIt->second;

			if (oldParentNode.m_firstChild == entity) {
				oldParentNode.m_firstChild = node.m_nextSibling;
			}
			if (oldParentNode.m_lastChild == entity) {
				oldParentNode.m_lastChild = node.m_previousSibling;
			}
		}

		// Update sibling links
		if (node.m_previousSibling != kNullEntity) {
			if (auto prevIt = m_impl->m_entities.find(node.m_previousSibling);
				prevIt != m_impl->m_entities.end()) {
				prevIt->second.m_nextSibling = node.m_nextSibling;
			}
		}
		if (node.m_nextSibling != kNullEntity) {
			if (auto nextIt = m_impl->m_entities.find(node.m_nextSibling);
				nextIt != m_impl->m_entities.end()) {
				nextIt->second.m_previousSibling = node.m_previousSibling;
			}
		}
	}

	// Clear sibling links
	node.m_previousSibling = kNullEntity;
	node.m_nextSibling = kNullEntity;

	// Set new parent
	node.m_parent = newParent;

	// Add to new parent's children
	auto& newParentNode = parentIt->second;
	if (newParentNode.m_firstChild == kNullEntity) {
		newParentNode.m_firstChild = entity;
		newParentNode.m_lastChild = entity;
	}
	else {
		if (auto lastChildIt = m_impl->m_entities.find(newParentNode.m_lastChild);
			lastChildIt != m_impl->m_entities.end()) {
			lastChildIt->second.m_nextSibling = entity;
			node.m_previousSibling = newParentNode.m_lastChild;
		}
		newParentNode.m_lastChild = entity;
	}
}

entity_t World::GetParent(entity_t entity) const {
	return m_impl->GetParent(entity);
}

entity_t World::GetNextSibling(entity_t entity) const {
	return m_impl->GetNextSibling(entity);
}

entity_t World::GetFirstChild(entity_t entity) const {
	return m_impl->GetFirstChild(entity);
}

// Iterator range implementations
EntityIteratorRange<EntityChildrenIterator> World::GetChildren(entity_t entity) const {
	entity_t firstChild = GetFirstChild(entity);
	return EntityIteratorRange<EntityChildrenIterator>(
		EntityChildrenIterator(this, firstChild),
		EntityChildrenIterator(this, kNullEntity)
	);
}

EntityIteratorRange<EntityAncestorIterator> World::GetAncestors(entity_t entity) const {
	entity_t parent = GetParent(entity);
	return EntityIteratorRange<EntityAncestorIterator>(
		EntityAncestorIterator(this, parent),
		EntityAncestorIterator(this, kNullEntity)
	);
}

EntityIteratorRange<EntityPrefixIterator> World::GetDescendants(entity_t entity) const {
	entity_t firstChild = GetFirstChild(entity);
	return EntityIteratorRange<EntityPrefixIterator>(
		EntityPrefixIterator(this, firstChild, entity),
		EntityPrefixIterator(this, kNullEntity, entity)
	);
}

// Iterator implementations
EntityChildrenIterator& EntityChildrenIterator::operator++() {
	if (m_current != kNullEntity) {
		m_current = m_world->GetNextSibling(m_current);
	}
	return *this;
}

EntityAncestorIterator& EntityAncestorIterator::operator++() {
	if (m_current != kNullEntity) {
		m_current = m_world->GetParent(m_current);
	}
	return *this;
}

EntityPrefixIterator& EntityPrefixIterator::operator++() {
	if (m_current == kNullEntity) {
		return *this;
	}

	// Try to go to first child
	entity_t firstChild = m_world->GetFirstChild(m_current);
	if (firstChild != kNullEntity) {
		m_current = firstChild;
		return *this;
	}

	// No children, try next sibling
	entity_t nextSibling = m_world->GetNextSibling(m_current);
	if (nextSibling != kNullEntity) {
		m_current = nextSibling;
		return *this;
	}

	// No sibling, go up the tree
	entity_t current = m_current;
	while (current != kNullEntity && current != m_root) {
		entity_t parent = m_world->GetParent(current);
		if (parent == kNullEntity || parent == m_root) {
			break;
		}

		entity_t parentSibling = m_world->GetNextSibling(parent);
		if (parentSibling != kNullEntity) {
			m_current = parentSibling;
			return *this;
		}

		current = parent;
	}

	m_current = kNullEntity;
	return *this;
}