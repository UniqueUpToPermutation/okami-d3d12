#pragma once

#include <memory>
#include <cstdint>

#include "engine.hpp"

namespace okami
{
	// Forward declarations
	struct WorldImpl;
	class World;

	struct EntityIteratorBase {
		const World* m_world;
		entity_t m_current;

		EntityIteratorBase(const World* world, entity_t current)
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
		EntityChildrenIterator(const World* world, entity_t current)
			: EntityIteratorBase(world, current) {}
		EntityChildrenIterator& operator++();
	};

	struct EntityAncestorIterator : public EntityIteratorBase {
		EntityAncestorIterator(const World* world, entity_t current)
			: EntityIteratorBase(world, current) {}
		EntityAncestorIterator& operator++();
	};

	struct EntityPrefixIterator : public EntityIteratorBase {
		entity_t m_root; // Root of the subtree we're traversing
		
		EntityPrefixIterator(const World* world, entity_t current, entity_t root)
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

	class World {
	private:
		std::unique_ptr<WorldImpl> m_impl;
		ISignalBus* m_signalBus = nullptr;

	public:
		World(ISignalBus* engine);
		World(Engine* engine);
		~World(); // Explicit destructor needed for unique_ptr with incomplete type
		
		// Move constructor and assignment (need explicit definitions for incomplete type)
		World(World&&) noexcept;
		World& operator=(World&&) noexcept;
		
		// Delete copy constructor and assignment (unique_ptr is not copyable)
		World(const World&) = delete;
		World& operator=(const World&) = delete;

		// Basic entity management
		entity_t CreateEntity(entity_t parent = kRoot);
		void RemoveEntity(entity_t entity);
		void SetParent(entity_t entity, entity_t parent = kRoot);

		// Hierarchy navigation
		entity_t GetParent(entity_t entity) const;
		EntityIteratorRange<EntityChildrenIterator> GetChildren(entity_t entity) const;
		EntityIteratorRange<EntityAncestorIterator> GetAncestors(entity_t entity) const;
		EntityIteratorRange<EntityPrefixIterator> GetDescendants(entity_t entity) const;

		// Public methods for iterator access (instead of friend classes)
		entity_t GetNextSibling(entity_t entity) const;
		entity_t GetFirstChild(entity_t entity) const;

		template <typename T>
		void AddComponent(entity_t entity, T component) {
			m_signalBus->Publish(ComponentAddSignal<T>(std::move(component)));
		}

		template <typename T>
		void RemoveComponent(entity_t entity) {
			m_signalBus->Publish(ComponentRemoveSignal<T>());
		}

		template <typename T>
		void UpdateComponent(entity_t entity, T component) {
			m_signalBus->Publish(ComponentUpdateSignal<T>(std::move(component)));
		}

	private:
		void AddEntity(entity_t entity, entity_t parent = kRoot);
	};
}