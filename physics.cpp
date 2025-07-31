#include "physics.hpp"
#include "storage.hpp"
#include "transform.hpp"

using namespace okami;

class PhysicsModule final : public IEngineModule {
public:
	void RegisterInterfaces(InterfaceCollection& queryable) override {
		m_storage.RegisterInterfaces(queryable);
	}

	void RegisterSignalHandlers(SignalHandlerCollection& eventBus) override {
		m_storage.RegisterSignalHandlers(eventBus);
	}

	Error Startup(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
		return {};
	}

	void Shutdown(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
	}

	void OnFrameBegin(Time const& time, ISignalBus& signalBus, EntityTree& world) override {
	}

	ModuleResult HandleSignals(Time const&, ISignalBus& signalBus) override {
		return m_storage.ProcessSignals();
	}

	std::string_view GetName() const override {
		return "Physics Module";
	}
private:
	Storage<Transform> m_storage;
};

std::unique_ptr<IEngineModule> PhysicsModuleFactory::operator()() {
	return std::make_unique<PhysicsModule>();
}