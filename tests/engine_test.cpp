#include <gtest/gtest.h>
#include "../engine.hpp"

using namespace okami;

class EngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<Engine>();
    }

    void TearDown() override {
        engine.reset();
    }

    std::unique_ptr<Engine> engine;
};

// Mock module for testing
class MockEngineModule : public IEngineModule {
private:
    bool m_startupCalled = false;
    bool m_shutdownCalled = false;
    int m_frameBeginCount = 0;
    int m_handleSignalsCount = 0;

public:
    void RegisterInterfaces(InterfaceCollection& queryable) override {
        // Mock implementation
    }

    void RegisterSignalHandlers(SignalHandlerCollection& eventBus) override {
        // Mock implementation
    }

    Error Startup(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
        m_startupCalled = true;
        return Error(); // No error
    }

    void Shutdown(IInterfaceQueryable& queryable, ISignalBus& eventBus) override {
        m_shutdownCalled = true;
    }

    void OnFrameBegin(Time const& time, ISignalBus& signalBus, EntityTree& world) override {
        m_frameBeginCount++;
    }

    ModuleResult HandleSignals(Time const&, ISignalBus& signalBus) override {
        m_handleSignalsCount++;
        return ModuleResult{true};
    }

    std::string_view GetName() const override {
        return "MockModule";
    }

    // Test accessors
    bool WasStartupCalled() const { return m_startupCalled; }
    bool WasShutdownCalled() const { return m_shutdownCalled; }
    int GetFrameBeginCount() const { return m_frameBeginCount; }
    int GetHandleSignalsCount() const { return m_handleSignalsCount; }
};

TEST_F(EngineTest, AddModuleTest) {
    // This test verifies that modules can be added to the engine
    // Since AddModule uses templates, we test the compilation succeeds
    auto& result = engine->AddModule<MockEngineModule>();
    EXPECT_EQ(&result, engine.get()); // Should return reference to engine for chaining
}

// Test ScopeGuard utility
TEST(UtilityTest, ScopeGuardTest) {
    bool executed = false;
    
    {
        OKAMI_DEFER(executed = true);
        EXPECT_FALSE(executed);
    }
    
    EXPECT_TRUE(executed);
}

TEST(UtilityTest, ScopeGuardDismissTest) {
    bool executed = false;
    
    {
        auto guard = ScopeGuard([&]() { executed = true; });
        guard.Dismiss();
    }
    
    EXPECT_FALSE(executed);
}

// Test Error class
TEST(ErrorTest, DefaultConstructorTest) {
    Error error;
    EXPECT_TRUE(error.IsOk());
    EXPECT_FALSE(error.IsError());
}

TEST(ErrorTest, StringConstructorTest) {
    Error error("Test error message");
    EXPECT_FALSE(error.IsOk());
    EXPECT_TRUE(error.IsError());
    EXPECT_EQ(error.Str(), "Test error message");
}

TEST(ErrorTest, StringViewConstructorTest) {
    const char* message = "Test error message";
    Error error(message);
    EXPECT_FALSE(error.IsOk());
    EXPECT_TRUE(error.IsError());
    EXPECT_EQ(error.Str(), message);
}

// Test InterfaceCollection
TEST(InterfaceCollectionTest, RegisterAndQueryTest) {
    InterfaceCollection collection;
    
    int testValue = 42;
    collection.Register<int>(&testValue);
    
    int* retrieved = collection.Query<int>();
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, 42);
    
    // Query for non-registered type should return nullptr
    float* notFound = collection.Query<float>();
    EXPECT_EQ(notFound, nullptr);
}

// Test SignalHandlerCollection
TEST(SignalHandlerCollectionTest, RegisterAndPublishTest) {
    SignalHandlerCollection signalBus;
    
    int receivedValue = 0;
    signalBus.RegisterHandler<int>([&](int value) {
        receivedValue = value;
    });
    
    signalBus.Publish(42);
    EXPECT_EQ(receivedValue, 42);
}

struct TestSignal {
    std::string message;
    int value;
};

TEST(SignalHandlerCollectionTest, CustomSignalTest) {
    SignalHandlerCollection signalBus;
    
    TestSignal received;
    signalBus.RegisterHandler<TestSignal>([&](TestSignal signal) {
        received = signal;
    });
    
    TestSignal sent{"Hello", 42};
    signalBus.Publish(sent);
    
    EXPECT_EQ(received.message, "Hello");
    EXPECT_EQ(received.value, 42);
}

// Test multiple handlers for same signal
TEST(SignalHandlerCollectionTest, MultipleHandlersTest) {
    SignalHandlerCollection signalBus;
    
    int handler1Count = 0;
    int handler2Count = 0;
    
    signalBus.RegisterHandler<int>([&](int) { handler1Count++; });
    signalBus.RegisterHandler<int>([&](int) { handler2Count++; });
    
    signalBus.Publish(42);
    
    EXPECT_EQ(handler1Count, 1);
    EXPECT_EQ(handler2Count, 1);
}