// Engine.h : Header file for the 'Engine' module.

#pragma once

#include <iostream>
#include <any>
#include <optional>
#include <variant>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>

#define OKAMI_DEFER(x) auto OKAMI_DEFER_##__LINE__ = okami::ScopeGuard([&]() { x; })

namespace okami {
	using entity_t = int32_t;
	constexpr entity_t kRoot = 0;
	constexpr entity_t kNullEntity = -1;

	class ScopeGuard {
	public:
		inline explicit ScopeGuard(std::function<void()> onExitScope)
			: m_onExitScope(std::move(onExitScope)), m_active(true) {
		}

		inline ScopeGuard(ScopeGuard&& other) noexcept
			: m_onExitScope(std::move(other.m_onExitScope)), m_active(other.m_active) {
			other.m_active = false;
		}

		inline ScopeGuard& operator=(ScopeGuard&& other) noexcept {
			if (this != &other) {
				if (m_active && m_onExitScope) m_onExitScope();
				m_onExitScope = std::move(other.m_onExitScope);
				m_active = other.m_active;
				other.m_active = false;
			}
			return *this;
		}

		inline ~ScopeGuard() {
			if (m_active && m_onExitScope) m_onExitScope();
		}

		// Prevent copy
		ScopeGuard(const ScopeGuard&) = delete;
		ScopeGuard& operator=(const ScopeGuard&) = delete;

		inline void Dismiss() noexcept { m_active = false; }

	private:
		std::function<void()> m_onExitScope;
		bool m_active;
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

	class ISignalBus {
	public:
		virtual void Publish(const std::type_info& eventType, std::any event) const = 0;

		template <typename T>
		void Publish(T event) const {
			Publish(typeid(T), std::make_any<T>(event));
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

	template <typename T>
	class ComponentAddSignal {
		T m_component;
	};

	template <typename T>
	class ComponentUpdateSignal {
		T m_component;
	};

	template <typename T>
	class ComponentRemoveSignal {
	};

	template <typename T>
	class IComponentHandler {
	public:
		virtual ~IComponentHandler() = default;

		virtual void Create(entity_t entity, T component) = 0;
		virtual void Update(entity_t entity, T component) = 0;
		virtual void Destroy(entity_t entity) = 0;
	};

	struct Error {
		std::variant<std::monostate, std::string_view, std::string> m_message;

		Error() : m_message(std::monostate{}) {}
		Error(const std::string& msg) : m_message(msg) {}
		Error(const char* msg) : m_message(std::string_view{ msg }) {}

		inline bool IsOk() const {
			return std::holds_alternative<std::monostate>(m_message);
		}

		inline bool IsError() const {
			return !IsOk();
		}

		friend std::ostream& operator<<(std::ostream& os, const Error& err) {
			os << err.Str();
			return os;
		}

		inline std::string Str() const {
			if (std::holds_alternative<std::string_view>(m_message)) {
				return std::string(std::get<std::string_view>(m_message));
			}
			else if (std::holds_alternative<std::string>(m_message)) {
				return std::get<std::string>(m_message);
			}
			return "No error message";
		}
	};

	struct ModuleResult {
		bool m_idle = true;
	};

	struct Time {
		double m_deltaTime;
		double m_totalTime;
	};

	class IEngineModule {
	public:
		virtual ~IEngineModule() = default;

		virtual void RegisterInterfaces(InterfaceCollection& queryable) = 0;
		virtual void RegisterSignalHandlers(ISignalBus& eventBus) = 0;

		virtual Error Startup(IInterfaceQueryable& queryable, ISignalBus& eventBus) = 0;
		virtual void Shutdown(IInterfaceQueryable& queryable, ISignalBus& eventBus) = 0;

		virtual void OnFrameBegin(Time const& time, ISignalBus& signalBus) = 0;
		virtual ModuleResult HandleSignals(Time const&, ISignalBus& signalBus) = 0;

		virtual std::string_view GetName() const = 0;
	};

	class IRenderer {
	public:
		virtual ~IRenderer() = default;

		virtual void Render() = 0;
		virtual void SaveToFile(const std::string& filename) = 0;
		virtual void SetHeadlessMode(bool headless) = 0;
	};

	struct SignalExit {};

	struct EngineParams {
		int m_argc = 0;
		const char** m_argv = nullptr;
		std::string_view m_configFilePath = "default.yaml";
		bool m_headlessMode = false;
		std::string_view m_headlessOutputFileStem = "output";
		std::optional<size_t> m_frameCount = std::nullopt;
	};

	class Engine final {
	private:
		EngineParams m_params;
		std::vector<std::unique_ptr<IEngineModule>> m_modules;
		InterfaceCollection m_interfaces;
		SignalHandlerCollection m_signalHandlers;

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
		void Run();
		void Shutdown();

		inline ISignalBus* GetSignalBus() {
			return &m_signalHandlers;
		}

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
}