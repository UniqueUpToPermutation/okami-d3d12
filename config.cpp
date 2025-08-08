#include "config.hpp"

#include <yaml-cpp/yaml.h>
#include <glog/logging.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <stack>

#define DEFAULT_PATH "config/default.yaml"

using namespace okami;

class ConfigModule;

class ConfigDeserializer : public IConfigDeserializer {
public:
    ConfigDeserializer(const YAML::Node& rootNode) : m_root(rootNode) {
        m_currentNode = m_root;
    }

    void Visit(std::string_view key, int& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsScalar()) {
                try {
                    value = node.as<int>();
                } catch (const YAML::Exception& e) {
                    LOG(WARNING) << "Failed to parse int value for key '" << key << "': " << e.what();
                }
            }
        }
    }

    void Visit(std::string_view key, double& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsScalar()) {
                try {
                    value = node.as<double>();
                } catch (const YAML::Exception& e) {
                    LOG(WARNING) << "Failed to parse double value for key '" << key << "': " << e.what();
                }
            }
        }
    }

    void Visit(std::string_view key, std::string& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsScalar()) {
                try {
                    value = node.as<std::string>();
                } catch (const YAML::Exception& e) {
                    LOG(WARNING) << "Failed to parse string value for key '" << key << "': " << e.what();
                }
            }
        }
    }

    void Visit(std::string_view key, bool& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsScalar()) {
                try {
                    value = node.as<bool>();
                } catch (const YAML::Exception& e) {
                    LOG(WARNING) << "Failed to parse bool value for key '" << key << "': " << e.what();
                }
            }
        }
    }

    void Visit(std::string_view key, std::vector<int>& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsSequence()) {
                value.clear();
                for (const auto& item : node) {
                    if (item.IsScalar()) {
                        try {
                            value.push_back(item.as<int>());
                        } catch (const YAML::Exception& e) {
                            LOG(WARNING) << "Failed to parse int array element for key '" << key << "': " << e.what();
                        }
                    }
                }
            }
        }
    }

    void Visit(std::string_view key, std::vector<double>& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsSequence()) {
                value.clear();
                for (const auto& item : node) {
                    if (item.IsScalar()) {
                        try {
                            value.push_back(item.as<double>());
                        } catch (const YAML::Exception& e) {
                            LOG(WARNING) << "Failed to parse double array element for key '" << key << "': " << e.what();
                        }
                    }
                }
            }
        }
    }

    void Visit(std::string_view key, std::vector<std::string>& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsSequence()) {
                value.clear();
                for (const auto& item : node) {
                    if (item.IsScalar()) {
                        try {
                            value.push_back(item.as<std::string>());
                        } catch (const YAML::Exception& e) {
                            LOG(WARNING) << "Failed to parse string array element for key '" << key << "': " << e.what();
                        }
                    }
                }
            }
        }
    }

    void Visit(std::string_view key, std::vector<bool>& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsSequence()) {
                value.clear();
                for (const auto& item : node) {
                    if (item.IsScalar()) {
                        try {
                            value.push_back(item.as<bool>());
                        } catch (const YAML::Exception& e) {
                            LOG(WARNING) << "Failed to parse bool array element for key '" << key << "': " << e.what();
                        }
                    }
                }
            }
        }
    }

    void Visit(std::string_view key, std::unordered_map<std::string, int>& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsMap()) {
                value.clear();
                for (const auto& pair : node) {
                    if (pair.first.IsScalar() && pair.second.IsScalar()) {
                        try {
                            value[pair.first.as<std::string>()] = pair.second.as<int>();
                        } catch (const YAML::Exception& e) {
                            LOG(WARNING) << "Failed to parse int map element for key '" << key << "': " << e.what();
                        }
                    }
                }
            }
        }
    }

    void Visit(std::string_view key, std::unordered_map<std::string, double>& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsMap()) {
                value.clear();
                for (const auto& pair : node) {
                    if (pair.first.IsScalar() && pair.second.IsScalar()) {
                        try {
                            value[pair.first.as<std::string>()] = pair.second.as<double>();
                        } catch (const YAML::Exception& e) {
                            LOG(WARNING) << "Failed to parse double map element for key '" << key << "': " << e.what();
                        }
                    }
                }
            }
        }
    }

    void Visit(std::string_view key, std::unordered_map<std::string, std::string>& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsMap()) {
                value.clear();
                for (const auto& pair : node) {
                    if (pair.first.IsScalar() && pair.second.IsScalar()) {
                        try {
                            value[pair.first.as<std::string>()] = pair.second.as<std::string>();
                        } catch (const YAML::Exception& e) {
                            LOG(WARNING) << "Failed to parse string map element for key '" << key << "': " << e.what();
                        }
                    }
                }
            }
        }
    }

    void Visit(std::string_view key, std::unordered_map<std::string, bool>& value) override {
        if (auto node = GetCurrentNode(key)) {
            if (node.IsMap()) {
                value.clear();
                for (const auto& pair : node) {
                    if (pair.first.IsScalar() && pair.second.IsScalar()) {
                        try {
                            value[pair.first.as<std::string>()] = pair.second.as<bool>();
                        } catch (const YAML::Exception& e) {
                            LOG(WARNING) << "Failed to parse bool map element for key '" << key << "': " << e.what();
                        }
                    }
                }
            }
        }
    }

    void EnterSection(std::string_view sectionName) override {
        m_sectionStack.push(m_currentNode);
        if (auto node = GetCurrentNode(sectionName)) {
            if (node.IsMap()) {
                m_currentNode = node;
            }
        }
    }

    void ExitSection() override {
        if (!m_sectionStack.empty()) {
            m_currentNode = m_sectionStack.top();
            m_sectionStack.pop();
        }
    }

private:
    YAML::Node GetCurrentNode(std::string_view key) {
        std::string keyStr(key);
        if (m_currentNode.IsMap() && m_currentNode[keyStr]) {
            return m_currentNode[keyStr];
        }
        return YAML::Node();
    }

    YAML::Node m_root;
    YAML::Node m_currentNode;
    std::stack<YAML::Node> m_sectionStack;
};

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

	void RegisterSignalHandlers(SignalHandlerCollection& eventBus) override {
	}

	Error Startup(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
        YAML::Node config;
        
        try {
            config = YAML::LoadFile(DEFAULT_PATH);
		}
		catch (const YAML::Exception& e) {
			return Error(std::string("Failed to load config: ") + e.what());
		}

		if (!config.IsMap()) {
			return Error("Configuration file is not a valid map structure.");
		}

        std::vector<std::string> configSections;
        for (auto const& configSection : config) {
            if (!configSection.first.IsScalar()) {
                LOG(WARNING) << "Skipping non-scalar key in config: " << configSection.first;
                continue;
            }

			configSections.push_back(configSection.first.as<std::string>());
        }

		// Create deserializers for each top-level section
        for (const auto& name : configSections) {
            if (auto it = m_deserializers.find(name); it != m_deserializers.end()) {
                try {
                    ConfigDeserializer deserializer(config[name]);
					auto result = (it->second)(deserializer);
                    m_configs[it->first] = result;
                } catch (const std::exception& e) {
                    LOG(WARNING) << "Failed to deserialize config section '" << name << "': " << e.what();
                }
            } else {
                LOG(WARNING) << "No deserializer registered for config section '" << name << "'";
			}
		}

		return {};
	}

	void Shutdown(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
	}

    void UploadResources() override {}

	void OnFrameBegin(Time const& time, ISignalBus& signalBus, EntityTree& entityTree) override {
        m_deserializers.clear();
        m_configs.clear();
	}

	ModuleResult HandleSignals(Time const&, ISignalBus& signalBus) override {
		return {};
	}

protected:
    void Register(std::string_view name, std::function<std::any(IConfigDeserializer&)> func) override {
		m_deserializers[name] = func;
    }

    std::optional<std::any> Read(std::string_view name) {
        if (auto it = m_configs.find(name); it != m_configs.end()) {
            return it->second;
		} else {
            return {};
		}
    }

private:
	std::unordered_map<std::string_view, std::function<std::any(IConfigDeserializer&)>> m_deserializers;
	std::unordered_map<std::string_view, std::any> m_configs;
};

std::unique_ptr<IEngineModule> ConfigModuleFactory::operator()() {
	return std::make_unique<ConfigModule>();
}