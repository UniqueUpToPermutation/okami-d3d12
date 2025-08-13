#include "physics.hpp"
#include "storage.hpp"
#include "transform.hpp"

using namespace okami;

class PhysicsModule final : public IEngineModule {
public:
	void Register(InterfaceCollection& queryable,
		SignalHandlerCollection& handlers) override {
		m_storage.RegisterInterfaces(queryable);
		m_storage.RegisterSignalHandlers(handlers);
	}

	Error Startup(InterfaceCollection& queryable, 
		SignalHandlerCollection& handlers, 
		ISignalBus& eventBus) override {
		return {};
	}

	void Shutdown(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
	}

	void OnFrameBegin(Time const& time, ISignalBus& signalBus, EntityTree& world) override {
	}

	void UploadResources() override {}

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