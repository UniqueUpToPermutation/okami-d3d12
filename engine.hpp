// Engine.h : Header file for the 'Engine' module.

#pragma once

#include <any>
#include <optional>
#include <variant>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <expected>
#include <string>
#include <filesystem>
#include <atomic>

#include "entity_tree.hpp"
#include "common.hpp"

namespace okami {
	template <typename T>
	class IStorageAccessor {
	public:
		virtual ~IStorageAccessor() = default;
		virtual T const* TryGet(entity_t entity) const = 0;
		inline T const& Get(entity_t entity) const {
			if (auto ptr = TryGet(entity); ptr) {
				return *ptr;
			}
			throw std::runtime_error("Entity not found in storage");
		}
		inline T GetOr(entity_t entity, T const& defaultValue) const {
			if (auto ptr = TryGet(entity); ptr) {
				return *ptr;
			}
			return defaultValue;
		}
	};

	class IInterfaceQueryable {
	public:
		~IInterfaceQueryable() = default;

		virtual std::optional<std::any> QueryType(std::type_info const& type) const = 0;

		template <typename T>
		T* Query() const {
			if (auto result = QueryType(typeid(T)); result) {
				return std::any_cast<T*>(*result);
			}
			else {
				return nullptr;
			}
		}

		template <typename T>
		IStorageAccessor<T>* QueryStorage() const {
			return Query<IStorageAccessor<T>>();
		}
	};

	class InterfaceCollection final : public IInterfaceQueryable {
	public:
		virtual ~InterfaceCollection() = default;

		template <typename T>
		void Register(T* interfacePtr) {
			m_interfaces[std::type_index(typeid(T))] = interfacePtr;
		}

		inline std::optional<std::any> QueryType(std::type_info const& type) const override {
			auto it = m_interfaces.find(type);
			if (it != m_interfaces.end()) {
				return it->second;
			}
			return std::nullopt;
		}

		auto begin() {
			return m_interfaces.begin();
		}

		auto end() {
			return m_interfaces.end();
		}

		auto cbegin() const {
			return m_interfaces.cbegin();
		}

		auto cend() const {
			return m_interfaces.cend();
		}

	protected:
		std::unordered_map<std::type_index, std::any> m_interfaces;
	};

	template <typename T>
	struct ComponentAddSignal {
		entity_t m_entity;
		T m_component;
	};

	template <typename T>
	struct ComponentUpdateSignal {
		entity_t m_entity;
		T m_component;
	};

	template <typename T>
	struct ComponentRemoveSignal {
		entity_t m_entity;
	};

	class ISignalBus {
	public:
		virtual void Publish(const std::type_info& eventType, std::any event) const = 0;

		template <typename T>
		void Publish(T event) const {
			Publish(typeid(T), std::make_any<T>(event));
		}

		template <typename T>
		void AddComponent(entity_t e, T component) const {
			Publish(ComponentAddSignal<T>{ e, std::move(component) });
		}

		template <typename T>
		void UpdateComponent(entity_t e, T component) const {
			Publish(ComponentUpdateSignal<T>{ e, std::move(component) });
		}

		template <typename T>
		void RemoveComponent(entity_t e) const {
			Publish(ComponentRemoveSignal<T>{e});
		}
	};

	template <typename T>
	class ISignalHandler {
	public:
		virtual ~ISignalHandler() = default;
		virtual void Handle(T event) = 0;
	};

	class SignalHandlerCollection final : public ISignalBus {
	private:
		class IAnySignalHandler {
		public:
			virtual ~IAnySignalHandler() = default;
			virtual void Handle(std::any event) = 0;
		};

		template <typename T>
		class AnySignalHandler final : public IAnySignalHandler {
		private:
			ISignalHandler<T>* m_handler;

		public:
			AnySignalHandler(ISignalHandler<T>* handler) : m_handler(handler) {}

			void Handle(std::any event) override {
				m_handler->Handle(std::any_cast<T>(event));
			}
		};

		std::unordered_multimap<std::type_index, std::function<void(std::any)>> m_eventHandlers;

	public:
		inline void Publish(const std::type_info& signalType, std::any signal) const override {
			auto range = m_eventHandlers.equal_range(std::type_index(signalType));
			for (auto it = range.first; it != range.second; ++it) {
				it->second(signal);
			}
		}

		template <typename T>
		void Publish(T signal) const {
			Publish(typeid(T), std::make_any<T>(std::move(signal)));
		}

		template <typename T>
		void RegisterHandler(std::function<void(T)> handler) {
			m_eventHandlers.emplace(std::type_index(typeid(T)), [handler](std::any sig) {
				handler(std::any_cast<T>(sig));
				});
		}
	};

	struct ModuleResult {
		bool m_idle = true;
		std::vector<Error> m_errors;

		ModuleResult& Union(ModuleResult const& other);
	};

	struct Time {
		double m_deltaTime;
		double m_totalTime;
		size_t m_frame;
	};

	class IEngineModule {
	public:
		virtual ~IEngineModule() = default;

		virtual void Register(
			InterfaceCollection& queryable, 
			SignalHandlerCollection& handlers) = 0;
		virtual Error Startup(InterfaceCollection& queryable, 
			SignalHandlerCollection& handlers, 
			ISignalBus& eventBus) = 0;
		virtual void Shutdown(IInterfaceQueryable& queryable, ISignalBus& eventBus) = 0;

		// Called at the very beginning of each frame, to allow the module
		// to finalize any resources 
		virtual void UploadResources() = 0;

		// Called at the beginning of each frame (after upload resources)
		// This is the only time when the module can modify the entity tree
		virtual void OnFrameBegin(Time const& time, ISignalBus& signalBus, EntityTree& entityTree) = 0;

		// Called continuously after OnFrameBegin to handle all signals generated during the frame
		virtual ModuleResult HandleSignals(Time const&, ISignalBus& signalBus) = 0;

		virtual std::string_view GetName() const = 0;
	};

	class IRenderer {
	public:
		virtual ~IRenderer() = default;

		virtual void Render() = 0;
		virtual void SaveToFile(const std::string& filename) = 0;
		virtual void SetHeadlessMode(bool headless) = 0;

		virtual void SetActiveCamera(entity_t e) = 0;
		virtual entity_t GetActiveCamera() const = 0;
	};

	struct SignalExit {};

	struct EngineParams {
		int m_argc = 0;
		const char** m_argv = nullptr;
		std::string_view m_configFilePath = "default.yaml";
		bool m_headlessMode = false;
		std::string_view m_headlessOutputFileStem = "output";
		bool m_forceLogToConsole = false;
	};

	using script_t = std::function<void(
		Time const& time,
		ISignalBus& signalBus,
		EntityTree& entityTree)>;

	using resource_id_t = int64_t;

	constexpr resource_id_t kInvalidResource = -1;

	template <typename T>
	concept ResourceType = requires {
		typename T::CreationData;
	};

	template <ResourceType T>
	struct Resource;

	template <ResourceType T>
	struct Resource {
		T m_data;
		resource_id_t m_id = kInvalidResource;
		std::string m_path;
		std::atomic<bool> m_loaded{ false };
		std::atomic<int> m_refCount{ 0 };
	};

	template <ResourceType T>
	struct ResHandle {
	private:
		Resource<T>* m_resource = nullptr;

	public:
		inline ResHandle() = default;
		inline ResHandle(Resource<T>* resource) : m_resource(resource) {
			if (m_resource) {
				m_resource->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}
		inline ResHandle(const ResHandle& other) : m_resource(other.m_resource) {
			if (m_resource) {
				m_resource->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}
		inline ResHandle& operator=(const ResHandle& other) {
			if (this != &other) {
				if (m_resource) {
					m_resource->m_refCount.fetch_sub(1, std::memory_order_relaxed);
				}
				m_resource = other.m_resource;
				if (m_resource) {
					m_resource->m_refCount.fetch_add(1, std::memory_order_relaxed);
				}
			}
			return *this;
		}
		inline ~ResHandle() {
			if (m_resource) {
				m_resource->m_refCount.fetch_sub(1, std::memory_order_relaxed);
			}
		}
		T const& operator*() const {
			if (!m_resource || !m_resource->m_loaded.load(std::memory_order_acquire)) {
				throw std::runtime_error("Resource not loaded");
			}
			return m_resource->m_data;
		}
		T const* operator->() const {
			if (!m_resource || !m_resource->m_loaded.load(std::memory_order_acquire)) {
				throw std::runtime_error("Resource not loaded");
			}
			return &m_resource->m_data;
		}
		inline T const& Get() const {
			return m_resource->m_data;
		}
		inline bool IsLoaded() const {
			return m_resource && m_resource->m_loaded.load(std::memory_order_acquire);
		}
		inline resource_id_t GetId() const {
			return m_resource ? m_resource->m_id : kInvalidResource;
		}
		inline std::string_view GetPath() const {
			return m_resource ? m_resource->m_path : std::string_view();
		}
		inline operator bool() const {
			return IsLoaded();
		}
	};

	template <ResourceType T>
	class IResourceManager {
	public:
		virtual ResHandle<T> Load(std::string_view path) = 0;
		virtual ResHandle<T> Create(typename T::CreationData&& data) = 0;

		inline ResHandle<T> Load(std::string const& path) {
			return Load(std::string_view(path));
		}
		inline ResHandle<T> Load(std::filesystem::path const& path) {
			return Load(path.string());
		}
	};

	class Engine final {
	private:
		EngineParams m_params;
		std::vector<std::unique_ptr<IEngineModule>> m_modules;
		InterfaceCollection m_interfaces;
		SignalHandlerCollection m_signalHandlers;
		EntityTree m_entityTree;

		std::atomic<bool> m_shouldExit{ false };

	public:
		template <typename T, typename... Args>
		Engine& AddModule(Args&&... args) {
			static_assert(std::is_base_of_v<IEngineModule, T>, "T must inherit from IEngineModule");
			m_modules.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
			return *this;
		}

		template <typename FactoryT, typename... Args>
		Engine& AddModuleFromFactory(Args&&... args) {
			m_modules.emplace_back(FactoryT{}(std::forward<Args>(args)...));
			return *this;
		}

		Error Startup();
		void UploadResources();
		void Run(std::optional<size_t> frameCount = std::nullopt);
		void Shutdown();

		inline EntityTree& GetEntityTree() {
			return m_entityTree;
		}

		inline ISignalBus const& GetSignalBus() const {
			return m_signalHandlers;
		}

		template <typename T>
		IStorageAccessor<T>* GetStorageAccessor() {
			return m_interfaces.Query<IStorageAccessor<T>>();
		}

		inline entity_t CreateEntity(entity_t parent = kRoot) {
			return m_entityTree.CreateEntity(m_signalHandlers, parent);
		}

		inline void RemoveEntity(entity_t entity) {
			m_entityTree.RemoveEntity(m_signalHandlers, entity);
		}

		inline void SetParent(entity_t entity, entity_t parent = kRoot) {
			m_entityTree.SetParent(m_signalHandlers, entity, parent);
		}

		template <typename T>
		void AddComponent(entity_t entity, T component) {
			m_signalHandlers.AddComponent(entity, std::move(component));
		}

		template <typename T>
		void UpdateComponent(entity_t entity, T component) {
			m_signalHandlers.UpdateComponent(entity, std::move(component));
		}

		template <typename T>
		void RemoveComponent(entity_t entity) {
			m_signalHandlers.RemoveComponent<T>(entity);
		}

		template <ResourceType T>
		IResourceManager<T>* GetResourceManager() {
			return m_interfaces.Query<IResourceManager<T>>();
		}

		/*
			Used for prototyping and scripting. Run a function every frame.
		*/
		void AddScript(script_t script, const std::string& name = "Unnamed");

		Engine(EngineParams params = {});
		~Engine();
	};

	// Define module factories
	struct D3D12RendererModuleFactory {
		std::unique_ptr<IEngineModule> operator()();
	};

	struct ConfigModuleFactory {
		std::unique_ptr<IEngineModule> operator()();
	};

	struct PhysicsModuleFactory {
		std::unique_ptr<IEngineModule> operator()();
	};
}