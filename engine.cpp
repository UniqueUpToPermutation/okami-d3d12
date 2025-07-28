// Engine.cpp : Defines the entry point for the application.
//

#include "engine.hpp"
#include "config.hpp"

#include <chrono>

#include <glog/logging.h>

using namespace okami;

Engine::Engine() {
	google::InitGoogleLogging("okami");
	AddModuleFromFactory<ConfigModuleFactory>();
}

Engine::~Engine() {
	Shutdown();
	google::ShutdownGoogleLogging();
}

Error Engine::Startup(int argc, char const* argv[]) {
	LOG(INFO) << "Starting Okami Engine";

	m_signalHandlers.RegisterHandler<SignalExit>([this](const SignalExit&) {
		m_shouldExit.store(true);
		});

	for (auto& module : m_modules) {
		module->RegisterInterfaces(m_interfaces);
		module->RegisterSignalHandlers(m_signalHandlers);
	}

	for (const auto& module : m_modules) {
		LOG(INFO) << "Starting module: " << module->GetName();
		auto error = module->Startup(m_interfaces, m_signalHandlers);
		if (error.IsError()) {
			LOG(ERROR) << "Failed to start module: " << module->GetName() << " - " << error;
			return error;
		}
	}

	return {};
}

void Engine::Shutdown() {
	LOG(INFO) << "Shutting down Okami Engine";
	for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it) {
		LOG(INFO) << "Shutting down module: " << (*it)->GetName();
		(*it)->Shutdown(m_interfaces, m_signalHandlers);
	}
	m_modules.clear();
}

void Engine::Run() {
	auto beginTick = std::chrono::high_resolution_clock::now();
	auto lastTick = beginTick;

	std::vector<bool> moduleFinished(m_modules.size(), false);

	auto* renderer = m_interfaces.Query<IRenderer>();

	if (!renderer) {
		LOG(WARNING) << "No renderer module found, running headless!";
	}

	while (!m_shouldExit.load()) {
		auto now = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> deltaTime = now - lastTick;
		std::chrono::duration<double> totalTime = now - beginTick;

		Time time{
			.m_deltaTime = deltaTime.count(),
			.m_totalTime = totalTime.count()
		};

		// Update frame
		auto updateFrame = [&]() {
			for (auto& module : m_modules) {
				module->OnFrameBegin(time, m_signalHandlers);
			}

			bool idle;
			do {
				idle = true;
				for (auto& module : m_modules) {
					auto result = module->HandleSignals(time, m_signalHandlers);
					idle &= result.m_idle;
				}
			} while (!idle);
		};

		updateFrame();

		// Render frame
		if (renderer) {
			renderer->Render();
		}
	}
}
