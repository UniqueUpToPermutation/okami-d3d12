#pragma once

#include "engine.hpp"
#include <type_traits>
#include <concepts>

#define OKAMI_CONFIG(TypeName) \
    constexpr std::string_view Name() const { \
        return #TypeName; \
    } \
    void Config(okami::IConfigDeserializer& deserializer) 

#define OKAMI_CONFIG_FIELD(name) \
    deserializer.Visit(#name, name);

namespace okami {
    class IConfigDeserializer;

    // Concept definition for ConfigStruct
    template <typename T>
    concept ConfigStruct = std::is_default_constructible_v<T> && requires(T obj, IConfigDeserializer& deserializer) {
        { obj.Config(deserializer) } -> std::same_as<void>;
		{ obj.Name() } -> std::convertible_to<std::string_view>;
    };

    class IConfigDeserializer {
    public:
        virtual ~IConfigDeserializer() = default;

        virtual void Visit(std::string_view key, int& value) = 0;
        virtual void Visit(std::string_view key, double& value) = 0;
        virtual void Visit(std::string_view key, std::string& value) = 0;
        virtual void Visit(std::string_view key, bool& value) = 0;

        virtual void Visit(std::string_view key, std::vector<int>& value) = 0;
        virtual void Visit(std::string_view key, std::vector<double>& value) = 0;
        virtual void Visit(std::string_view key, std::vector<std::string>& value) = 0;
        virtual void Visit(std::string_view key, std::vector<bool>& value) = 0;

        virtual void Visit(std::string_view key, std::unordered_map<std::string, int>& value) = 0;
        virtual void Visit(std::string_view key, std::unordered_map<std::string, double>& value) = 0;
        virtual void Visit(std::string_view key, std::unordered_map<std::string, std::string>& value) = 0;
        virtual void Visit(std::string_view key, std::unordered_map<std::string, bool>& value) = 0;

        virtual void EnterSection(std::string_view sectionName) = 0;
        virtual void ExitSection() = 0;

        template <ConfigStruct T>
        void Visit(std::string_view key, T& value) {
            EnterSection(key);
            value.Config(*this);
            ExitSection();
        }
    };

    class IConfigModule {
    protected:
        virtual void Register(std::string_view name, std::function<std::any(IConfigDeserializer&)> func) = 0;
        virtual std::optional<std::any> Read(std::string_view name) = 0;

    public:
        virtual ~IConfigModule() = default;

        template <typename T>
        void Register() {
            Register(T{}.Name(), [](IConfigDeserializer& deserializer) {
                T value;
				value.Config(deserializer);
                return value;
			});
        }

        template <ConfigStruct T>
        T Read() {
            T value;
            if (auto result = Read(T{}.Name()); result) {
                value = std::any_cast<T>(*result);
			}
            return value;
		}
    };

	template <ConfigStruct T, typename Logger>
    T ReadConfig(IInterfaceQueryable& queryable, Logger& log) {
        if (auto configModule = queryable.Query<IConfigModule>()) {
            return configModule->Read<T>();
        } else {
			log << "Config module not found for type: " << T{}.Name() << "\n";
            return T{};
        }
	}

    template <ConfigStruct T, typename Logger>
    void RegisterConfig(IInterfaceQueryable& queryable, Logger& log) {
        if (auto configModule = queryable.Query<IConfigModule>()) {
            configModule->Register<T>();
        } else {
            log << "Config module not found for type: " << T{}.Name() << "\n";
        }
	}
}