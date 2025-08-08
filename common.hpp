#pragma once

#include <stdexcept>
#include <expected>
#include <functional>
#include <variant>
#include <string_view>
#include <string>
#include <iostream>

#define OKAMI_NO_COPY(x) \
	x(const x&) = delete; \
	x& operator=(const x&) = delete; \

#define OKAMI_NO_MOVE(x) \
	x(x&&) = delete; \
	x& operator=(x&&) = delete; \

#define OKAMI_MOVE(x) \
	x(x&&) = default; \
	x& operator=(x&&) = default; \

#ifndef NDEBUG 
#define OKAMI_ASSERT(condition, message) \
	do { \
		if (!(condition)) { \
			throw std::runtime_error(message); \
		} \
	} while (false) 
#else
#define OKAMI_ASSERT(condition, message) 
#endif

#define OKAMI_DEFER(x) auto OKAMI_DEFER_##__LINE__ = okami::ScopeGuard([&]() { x; })

namespace okami {
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

	template <typename T>
	using Expected = std::expected<T, Error>;

	template <typename T>
	struct TypeWrapper {
		using Type = T;
	};

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
}