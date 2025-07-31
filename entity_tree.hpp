#pragma once

#include <memory>
#include <cstdint>

namespace okami
{
	class ISignalBus;
	class Engine;

	using entity_t = int32_t;
	constexpr entity_t kRoot = 0;
	constexpr entity_t kNullEntity = -1;

	struct EntityCreateSignal {
		entity_t m_entity;
		entity_t m_parent;
	};

	struct EntityRemoveSignal {
		entity_t m_entity;
	};

	struct EntityParentChangeSignal {
		entity_t m_entity;
		entity_t m_oldParent;
		entity_t m_newParent;
	};

	// Forward declarations
	struct EntityTreeImpl;
	class EntityTree;

	struct EntityIteratorBase {
		const EntityTree* m_world;
		entity_t m_current;

		EntityIteratorBase(const EntityTree* world, entity_t current)
			: m_world(world), m_current(current) {}

		operator entity_t() const {
			return m_current;
		}
		entity_t operator*() const {
			return m_current;
		}
		bool operator==(const EntityIteratorBase& other) const {
			return m_current == other.m_current;
		}
	};

	struct EntityChildrenIterator : public EntityIteratorBase {
		EntityChildrenIterator(const EntityTree* world, entity_t current)
			: EntityIteratorBase(world, current) {}
		EntityChildrenIterator& operator++();
	};

	struct EntityAncestorIterator : public EntityIteratorBase {
		EntityAncestorIterator(const EntityTree* world, entity_t current)
			: EntityIteratorBase(world, current) {}
		EntityAncestorIterator& operator++();
	};

	struct EntityPrefixIterator : public EntityIteratorBase {
		entity_t m_root; // Root of the subtree we're traversing
		
		EntityPrefixIterator(const EntityTree* world, entity_t current, entity_t root)
			: EntityIteratorBase(world, current), m_root(root) {}
		EntityPrefixIterator& operator++();
	};

	template <typename T>
	struct EntityIteratorRange {
		T m_begin;
		T m_end;

		EntityIteratorRange(T begin, T end) : m_begin(begin), m_end(end) {}
		T begin() const { return m_begin; }
		T end() const { return m_end; }
	};

	struct EntityTreeImplDeleter {
		void operator()(EntityTreeImpl* impl) const;
	};;

	class EntityTree {
	private:
		std::unique_ptr<EntityTreeImpl, EntityTreeImplDeleter> m_impl;

	public:
		EntityTree();

		// Basic entity management
		entity_t CreateEntity(ISignalBus const& signalBus, entity_t parent = kRoot);
		void RemoveEntity(ISignalBus const& signalBus, entity_t entity);
		void SetParent(ISignalBus const& signalBus, entity_t entity, entity_t parent = kRoot);

		// Hierarchy navigation
		entity_t GetParent(entity_t entity) const;
		EntityIteratorRange<EntityChildrenIterator> GetChildren(entity_t entity) const;
		EntityIteratorRange<EntityAncestorIterator> GetAncestors(entity_t entity) const;
		EntityIteratorRange<EntityPrefixIterator> GetDescendants(entity_t entity) const;

		// Public methods for iterator access (instead of friend classes)
		entity_t GetNextSibling(entity_t entity) const;
		entity_t GetFirstChild(entity_t entity) const;

	private:
		void AddEntity(entity_t entity, entity_t parent = kRoot);
	};
}