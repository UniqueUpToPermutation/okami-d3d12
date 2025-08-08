#pragma once

#include "engine.hpp"

#include <queue>
#include <sstream>

namespace okami {
	template <typename... Ts>
	struct Storage;

	template <typename T, typename... Ts>
	struct StorageAccessor : public IStorageAccessor<T> {
		Storage<Ts...>* m_storage;

		StorageAccessor(Storage<Ts...>* storage) : m_storage(storage) {}

		T const* TryGet(entity_t entity) const override;
	};

	template <typename... Ts>
	struct Storage {
		std::tuple<std::queue<ComponentUpdateSignal<Ts>>...> updateSignals;
		std::tuple<std::queue<ComponentAddSignal<Ts>>...> addSignals;
		std::tuple<std::queue<ComponentRemoveSignal<Ts>>...> removeSignals;
		std::queue<EntityRemoveSignal> entityRemoveSignals;

		std::tuple<StorageAccessor<Ts, Ts...>...> accessors = std::make_tuple(
			StorageAccessor<Ts, Ts...>(this)...);

		std::tuple<std::function<void(entity_t, Ts const&, Ts const&)>...> updateCallbacks;
		std::tuple<std::function<void(entity_t, Ts const&)>...> addCallbacks;
		std::tuple<std::function<void(entity_t, Ts const&)>...> removeCallbacks;

		std::tuple<std::unordered_map<entity_t, Ts>...> data;

		Storage() = default;

		// Disable copy and move operations
		// Move must be disabled to prevent dangling pointers in accessors
		OKAMI_NO_COPY(Storage);
		OKAMI_NO_MOVE(Storage);

		template <typename T>
		std::unordered_map<entity_t, T>& GetStorage() {
			return std::get<std::unordered_map<entity_t, T>>(data);
		}

		template <typename T>
		std::unordered_map<entity_t, T> const& GetStorage() const {
			return std::get<std::unordered_map<entity_t, T>>(data);
		}

		void RegisterSignalHandlers(SignalHandlerCollection& collection) {
			auto registerForType = [this, &collection](auto typeWrapper) {
				using T = typename decltype(typeWrapper)::Type;

				collection.RegisterHandler<ComponentAddSignal<T>>(
					[this](ComponentAddSignal<T> signal) {
						std::get<std::queue<ComponentAddSignal<T>>>(addSignals).push(std::move(signal));
					}
				);
				collection.RegisterHandler<ComponentUpdateSignal<T>>(
					[this](ComponentUpdateSignal<T> signal) {
						std::get<std::queue<ComponentUpdateSignal<T>>>(updateSignals).push(std::move(signal));
					}
				);
				collection.RegisterHandler<ComponentRemoveSignal<T>>(
					[this](ComponentRemoveSignal<T> signal) {
						std::get<std::queue<ComponentRemoveSignal<T>>>(removeSignals).push(std::move(signal));
					}
				);
				};

			(registerForType(TypeWrapper<Ts>{}), ...);

			collection.RegisterHandler<EntityRemoveSignal>(
				[this](EntityRemoveSignal signal) {
					entityRemoveSignals.push(std::move(signal));
				}
			);
		}

		void RegisterInterfaces(InterfaceCollection& queryable) {
			// Register storage interfaces for each component type
			auto registerForType = [this, &queryable](auto typeWrapper) {
				using T = typename decltype(typeWrapper)::Type;
				queryable.Register<IStorageAccessor<T>>(&std::get<StorageAccessor<T, Ts...>>(accessors));
			};
			(registerForType(TypeWrapper<Ts>{}), ...);
		}

		ModuleResult ProcessSignals() {
			std::vector<Error> errors;
			bool hasSignals = false;

			auto processForType = [this, &errors, &hasSignals](auto typeWrapper) {
				using T = typename decltype(typeWrapper)::Type;

				// Process add signals
				auto& addQueue = std::get<std::queue<ComponentAddSignal<T>>>(addSignals);
				while (!addQueue.empty()) {
					auto signal = std::move(addQueue.front());
					addQueue.pop();
					auto& storage = GetStorage<T>();
					auto it = storage.find(signal.m_entity);
					if (it != storage.end()) {
						std::stringstream ss;
						ss << "Entity " << signal.m_entity << " already has component of type " << typeid(T).name();
						errors.push_back(Error(ss.str()));
					}
					else {
						auto resultIt = storage.emplace_hint(it, signal.m_entity, std::move(signal.m_component));
						if (auto& callback = std::get<std::function<void(entity_t, T const&)>>(addCallbacks)) {
							callback(signal.m_entity, resultIt->second);
						}
					}
					hasSignals = true;
				}
				// Process update signals
				auto& updateQueue = std::get<std::queue<ComponentUpdateSignal<T>>>(updateSignals);
				while (!updateQueue.empty()) {
					auto signal = std::move(updateQueue.front());
					updateQueue.pop();
					auto& storage = GetStorage<T>();
					auto it = storage.find(signal.m_entity);
					if (it == storage.end()) {
						std::stringstream ss;
						ss << "Entity " << signal.m_entity << " does not have component of type " << typeid(T).name();
						errors.push_back(Error(ss.str()));
					} else {
						std::swap(it->second, signal.m_component);
						if (auto& callback = std::get<std::function<void(entity_t, T const&, T const&)>>(updateCallbacks)) {
							callback(signal.m_entity, signal.m_component, it->second);
						}
					}
					hasSignals = true;
				}
				// Process remove signals
				auto& removeQueue = std::get<std::queue<ComponentRemoveSignal<T>>>(removeSignals);
				while (!removeQueue.empty()) {
					auto signal = std::move(removeQueue.front());
					removeQueue.pop();
					auto& storage = GetStorage<T>();
					auto it = storage.find(signal.m_entity);
					if (it == storage.end()) {
						std::stringstream ss;
						ss << "Entity " << signal.m_entity << " does not have component of type " << typeid(T).name();
						errors.push_back(Error(ss.str()));
					}
					else {
						auto data = std::move(it->second);
						storage.erase(it);
						if (auto& callback = std::get<std::function<void(entity_t, T const&)>>(removeCallbacks)) {
							callback(signal.m_entity, data);
						}
					}
					hasSignals = true;
				}
				};

			(processForType(TypeWrapper<Ts>{}), ...);

			auto processEntityRemovedForType = [this, &hasSignals](auto typeWrapper, entity_t e) {
				using T = typename decltype(typeWrapper)::Type;
				auto& storage = GetStorage<T>();
				auto it = storage.find(e);
				if (it != storage.end()) {
					if (auto& callback = std::get<std::function<void(entity_t, T const&)>>(removeCallbacks)) {
						callback(it->first, it->second);
					}
					storage.erase(it);
				}
				hasSignals = true;
			};

			while (!entityRemoveSignals.empty()) {
				auto signal = std::move(entityRemoveSignals.front());
				entityRemoveSignals.pop();
				(processEntityRemovedForType(TypeWrapper<Ts>{}, signal.m_entity), ...);
			}

			return ModuleResult{
				.m_idle = !hasSignals,
				.m_errors = std::move(errors) 
			};
		}

		void Clear() {
			auto clearForType = [this](auto typeWrapper) {
				using T = typename decltype(typeWrapper)::Type;
				GetStorage<T>().clear();
			};
			(clearForType(TypeWrapper<Ts>{}), ...);
		}
	};

	template <typename T, typename... Ts>
	T const* StorageAccessor<T, Ts...>::TryGet(entity_t entity) const {
		auto& storage = m_storage->GetStorage<T>();
		auto it = storage.find(entity);
		if (it != storage.end()) {
			return &it->second;
		}
		return nullptr;
	}
}