#include "config.hpp"

#include <yaml-cpp/yaml.h>
#include <glog/logging.h>

#define DEFAULT_PATH "config/default.yaml"

using namespace okami;

class ConfigModule final :
	public IConfigModule,
	public IEngineModule {
public:
	ConfigModule() {
	}

	std::string_view GetName() const override {
		return "Configuration Module";
	}

	void RegisterInterfaces(InterfaceCollection& queryable) override {
		queryable.Register<IConfigModule>(this);
	}

	void RegisterSignalHandlers(ISignalBus& eventBus) override {
	}

	void Parse(YAML::Node const& node, std::string const& currentPrefix = "") {
		if (node.IsMap()) {
			for (const auto& it : node) {
				const auto& key = it.first.as<std::string>();
				const auto& value = it.second;
				if (value.IsScalar()) {
					m_configData[currentPrefix + key] = value.as<std::string>();
				}
				else if (value.IsSequence()) {
					LOG(WARNING) << "Sequences are not supported in config parsing: " << currentPrefix + key;
				}
				else if (value.IsMap()) {
					Parse(value, currentPrefix + key + ".");
				}
			}
		}
	}

	Error Startup(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
		YAML::Node config;
		try {
			config = YAML::LoadFile(DEFAULT_PATH);
		}
		catch (const YAML::Exception& e) {
			return Error(std::string("Failed to load config: ") + e.what());
		}

		Parse(config);

		return {};
	}

	void Shutdown(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
	}

	void OnFrameBegin(Time const& time, ISignalBus& signalBus) override {
		// No specific actions needed at the start of a frame for config module
	}

	ModuleResult HandleSignals(Time const&, ISignalBus& signalBus) override {
		return {};
	}

	std::optional<int> GetInt(std::string_view key) const override {
		auto it = m_configData.find(std::string(key));
		if (it != m_configData.end()) {
			try {
				return std::stoi(it->second);
			}
			catch (...) {
				return std::nullopt;
			}
		}
		return std::nullopt;
	}

	std::optional<float> GetFloat(std::string_view key) const override {
		auto it = m_configData.find(std::string(key));
		if (it != m_configData.end()) {
			try {
				return std::stof(it->second);
			}
			catch (...) {
				return std::nullopt;
			}
		}
		return std::nullopt;
	}

	std::optional<std::string> GetString(std::string_view key) const override {
		auto it = m_configData.find(std::string(key));
		if (it != m_configData.end()) {
			return it->second;
		}
		return std::nullopt;
	}

private:
	std::unordered_map<std::string, std::string> m_configData;
};

std::unique_ptr<IEngineModule> ConfigModuleFactory::operator()() {
	return std::make_unique<ConfigModule>();
}