#include "entity_tree.hpp"
#include <unordered_map>
#include <iostream>
#include "assert.hpp"

#include "engine.hpp"
#include "pool.hpp"

using namespace okami;

// Define WorldNode and WorldImpl in the .cpp file to keep them private
struct EntityTreeNode {
	entity_t m_entityId = kNullEntity;
	entity_t m_parent = kNullEntity;
	entity_t m_firstChild = kNullEntity;
	entity_t m_lastChild = kNullEntity;
	entity_t m_nextSibling = kNullEntity;
	entity_t m_previousSibling = kNullEntity;

	EntityTreeNode(entity_t id) : m_entityId(id) {}
	EntityTreeNode() = default; // Default constructor for empty nodes
};

namespace okami {
	struct EntityTreeImpl {
		Pool<EntityTreeNode, entity_t> m_entityPool;
	};
}

void EntityTreeImplDeleter::operator()(EntityTreeImpl* impl) const {
	delete impl;
}

EntityTree::EntityTree() : m_impl(std::unique_ptr<EntityTreeImpl, EntityTreeImplDeleter>(new EntityTreeImpl())) {
	auto root = m_impl->m_entityPool.Allocate();
	OKAMI_ASSERT(root == kRoot, "Root entity should always be at index 0");
}

entity_t EntityTree::CreateEntity(entity_t parent) {
	entity_t newId = m_impl->m_entityPool.Allocate();
	AddEntity(newId, parent);

	OKAMI_ASSERT(m_signalBus != nullptr, "BeginUpdates must be called before CreateEntity");
	m_signalBus->Publish(EntityCreateSignal{ .m_entity = newId, .m_parent = parent });

	return newId;
}

void EntityTree::AddEntity(entity_t entity, entity_t parent) {
	OKAMI_ASSERT(!m_impl->m_entityPool.IsFree(parent), "Parent entity must exist in the tree");

	EntityTreeNode newNode(entity);
	newNode.m_parent = parent;

	auto& parentNode = m_impl->m_entityPool[parent];
	if (parentNode.m_firstChild == kNullEntity) {
		parentNode.m_firstChild = entity;
		parentNode.m_lastChild = entity;
	}
	else {
		// Link to the last child
		if (parentNode.m_lastChild != kNullEntity) {
			newNode.m_previousSibling = parentNode.m_lastChild;
			m_impl->m_entityPool[parentNode.m_lastChild].m_nextSibling = entity;
			parentNode.m_lastChild = entity;
		}
	}

	// Update the pool with the new node
	m_impl->m_entityPool[entity] = newNode;
}

void EntityTree::RemoveEntity(entity_t entity) {
	if (entity == kRoot) {
		throw std::runtime_error("Cannot remove root entity");
	}

	OKAMI_ASSERT(m_impl->m_entityPool.IsFree(entity) == false, "Entity must exist in the tree");
	const auto& node = m_impl->m_entityPool[entity];

	// Remove children recursively
	entity_t child = node.m_firstChild;
	while (child != kNullEntity) {
		auto nextChild = m_impl->m_entityPool[child].m_nextSibling;
		RemoveEntity(child);
		child = nextChild;
	}

	// Update parent's child pointers
	if (node.m_parent != kNullEntity) {
		auto& parent = m_impl->m_entityPool[node.m_parent];
		if (parent.m_firstChild == entity) {
			parent.m_firstChild = node.m_nextSibling;
		}
		if (parent.m_lastChild == entity) {
			parent.m_lastChild = node.m_previousSibling;
		}
	}

	// Update sibling links
	if (node.m_previousSibling != kNullEntity) {
		m_impl->m_entityPool[node.m_previousSibling].m_nextSibling = node.m_nextSibling;
	}
	if (node.m_nextSibling != kNullEntity) {
		m_impl->m_entityPool[node.m_nextSibling].m_previousSibling = node.m_previousSibling;
	}

	m_impl->m_entityPool.Free(entity);

	OKAMI_ASSERT(m_signalBus != nullptr, "BeginUpdates must be called before RemoveEntity");
	m_signalBus->Publish(EntityRemoveSignal{ .m_entity = entity });
}

void EntityTree::BeginUpdates(ISignalBus const& signalBus) {
	m_signalBus = &signalBus;
}

void EntityTree::EndUpdates() {
	m_signalBus = nullptr;
}

void EntityTree::SetParent(entity_t entity, entity_t newParent) {
	// Cannot reparent the root entity
	if (entity == kRoot) {
		throw std::runtime_error("Cannot reparent root entity");
	}

	// Validate entities exist
	OKAMI_ASSERT(!m_impl->m_entityPool.IsFree(entity), "Entity must exist in the tree");
	OKAMI_ASSERT(!m_impl->m_entityPool.IsFree(newParent), "New parent entity must exist in the tree");

	entity_t oldParent = m_impl->m_entityPool[entity].m_parent;

	// If already the parent, do nothing
	if (oldParent == newParent) {
		return;
	}

	// Check for circular dependency - newParent cannot be a descendant of entity
	entity_t checkParent = newParent;
	while (checkParent != kNullEntity) {
		if (checkParent == entity) {
			// Circular dependency detected, do nothing
			return;
		}
		checkParent = m_impl->m_entityPool[checkParent].m_parent;
	}

	// Remove entity from old parent's child list
	auto& entityNode = m_impl->m_entityPool[entity];
	if (oldParent != kNullEntity) {
		auto& oldParentNode = m_impl->m_entityPool[oldParent];
		
		// Update parent's first/last child pointers
		if (oldParentNode.m_firstChild == entity) {
			oldParentNode.m_firstChild = entityNode.m_nextSibling;
		}
		if (oldParentNode.m_lastChild == entity) {
			oldParentNode.m_lastChild = entityNode.m_previousSibling;
		}
	}

	// Update sibling links in old parent
	if (entityNode.m_previousSibling != kNullEntity) {
		m_impl->m_entityPool[entityNode.m_previousSibling].m_nextSibling = entityNode.m_nextSibling;
	}
	if (entityNode.m_nextSibling != kNullEntity) {
		m_impl->m_entityPool[entityNode.m_nextSibling].m_previousSibling = entityNode.m_previousSibling;
	}

	// Clear entity's sibling links
	entityNode.m_nextSibling = kNullEntity;
	entityNode.m_previousSibling = kNullEntity;

	// Set new parent
	entityNode.m_parent = newParent;

	// Add entity to new parent's child list
	auto& newParentNode = m_impl->m_entityPool[newParent];
	if (newParentNode.m_firstChild == kNullEntity) {
		// First child
		newParentNode.m_firstChild = entity;
		newParentNode.m_lastChild = entity;
	} else {
		// Add as last child
		if (newParentNode.m_lastChild != kNullEntity) {
			entityNode.m_previousSibling = newParentNode.m_lastChild;
			m_impl->m_entityPool[newParentNode.m_lastChild].m_nextSibling = entity;
		}
		newParentNode.m_lastChild = entity;
	}

	OKAMI_ASSERT(m_signalBus != nullptr, "BeginUpdates must be called before SetParent");
	m_signalBus->Publish(EntityParentChangeSignal{.m_entity = entity, .m_oldParent = oldParent, .m_newParent = newParent});
}

entity_t EntityTree::GetParent(entity_t entity) const {
	OKAMI_ASSERT(m_impl != nullptr, "EntityTreeImpl must be initialized");
	return m_impl->m_entityPool[entity].m_parent;
}

entity_t EntityTree::GetNextSibling(entity_t entity) const {
	OKAMI_ASSERT(m_impl != nullptr, "EntityTreeImpl must be initialized");
	return m_impl->m_entityPool[entity].m_nextSibling;
}

entity_t EntityTree::GetFirstChild(entity_t entity) const {
	OKAMI_ASSERT(m_impl != nullptr, "EntityTreeImpl must be initialized");
	return m_impl->m_entityPool[entity].m_firstChild;
}

// Iterator range implementations
EntityIteratorRange<EntityChildrenIterator> EntityTree::GetChildren(entity_t entity) const {
	entity_t firstChild = GetFirstChild(entity);
	return EntityIteratorRange<EntityChildrenIterator>(
		EntityChildrenIterator(this, firstChild),
		EntityChildrenIterator(this, kNullEntity)
	);
}

EntityIteratorRange<EntityAncestorIterator> EntityTree::GetAncestors(entity_t entity) const {
	entity_t parent = GetParent(entity);
	return EntityIteratorRange<EntityAncestorIterator>(
		EntityAncestorIterator(this, parent),
		EntityAncestorIterator(this, kNullEntity)
	);
}

EntityIteratorRange<EntityPrefixIterator> EntityTree::GetDescendants(entity_t entity) const {
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