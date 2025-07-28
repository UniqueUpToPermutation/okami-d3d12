// Engine.cpp : Defines the entry point for the application.
//

#include "engine.hpp"
#include "config.hpp"

#include <chrono>

#include <glog/logging.h>

using namespace okami;

Engine::Engine(EngineParams params) : m_params(params) {
	std::string name;
	if (m_params.m_argc == 0) {
		name = "okami";
	} else {
		name = m_params.m_argv[0];
	}
	google::InitGoogleLogging(name.c_str());
	AddModuleFromFactory<ConfigModuleFactory>();
}

Engine::~Engine() {
	Shutdown();
	google::ShutdownGoogleLogging();
}

Error Engine::Startup() {
	LOG(INFO) << "Starting Okami Engine";

	m_signalHandlers.RegisterHandler<SignalExit>([this](const SignalExit&) {
		m_shouldExit.store(true);
		});

	for (auto& module : m_modules) {
		module->RegisterInterfaces(m_interfaces);
		module->RegisterSignalHandlers(m_signalHandlers);
	}

	if (auto* renderer = m_interfaces.Query<IRenderer>(); renderer) {
		renderer->SetHeadlessMode(m_params.m_headlessMode);
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
	auto* config = m_interfaces.Query<IConfigModule>();

	std::optional<size_t> maxFrames = m_params.m_frameCount;
	bool headlessMode = m_params.m_headlessMode;

	if (!renderer) {
		LOG(WARNING) << "No renderer module found, running headless!";
		headlessMode = true;
	}

	if (headlessMode && !maxFrames) {
		maxFrames = 1; // Default to 1 frames in headless mode
		LOG(INFO) << "Running in headless mode, defaulting to " << *maxFrames << " frames.";
	}

	size_t frameCount = 0;

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

		if (m_params.m_headlessMode) {
			std::string outputFile = std::string(m_params.m_headlessOutputFileStem) + "_" + std::to_string(frameCount) + ".dds";
			LOG(INFO) << "Saving headless frame to: " << outputFile;
			renderer->SaveToFile(outputFile);
		}
	
		frameCount++;
		if (maxFrames && frameCount >= *maxFrames) {
			m_shouldExit.store(true);
		}

		lastTick = now;
	}
}

