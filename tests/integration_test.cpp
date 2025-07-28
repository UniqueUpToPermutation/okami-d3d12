#include <gtest/gtest.h>
#include <gtest/gtest-param-test.h>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include "../world.hpp"

using namespace okami;

// Integration tests that test multiple components working together

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for integration tests
    }

    void TearDown() override {
        // Cleanup for integration tests
    }
};

// Parameterized test example
class ParameterizedTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        value = GetParam();
    }

    int value;
};

// Test with multiple parameter values
TEST_P(ParameterizedTest, MultipleValuesTest) {
    EXPECT_GE(value, 0);
    EXPECT_LE(value, 100);
}

// Instantiate the test with different parameters
INSTANTIATE_TEST_SUITE_P(
    ValueRange,
    ParameterizedTest,
    ::testing::Values(0, 25, 50, 75, 100)
);

// Performance/Stress Tests
TEST(PerformanceTest, WorldHierarchyPerformanceTest) {
    // Test creating large hierarchies
    SignalHandlerCollection signalHandlers;
    World world(&signalHandlers);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create 1000 entities in a deep hierarchy
    entity_t current = world.CreateEntity();
    for (int i = 1; i < 1000; ++i) {
        entity_t next = world.CreateEntity(current);
        current = next;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (adjust as needed)
    EXPECT_LT(duration.count(), 1000); // Less than 1 second
    
    // Test ancestor traversal performance
    start = std::chrono::high_resolution_clock::now();
    
    int ancestorCount = 0;
    for (auto ancestor : world.GetAncestors(current)) {
        ancestorCount++;
    }
    
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(ancestorCount, 1000); // Should find all ancestors + root
    EXPECT_LT(duration.count(), 100); // Should be fast
}

TEST(PerformanceTest, WorldBroadHierarchyPerformanceTest) {
    // Test creating wide hierarchies
	SignalHandlerCollection signalHandlers;
    World world(&signalHandlers);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    entity_t root = world.CreateEntity();
    
    // Create 1000 children under one parent
    std::vector<entity_t> children;
    for (int i = 0; i < 1000; ++i) {
        children.push_back(world.CreateEntity(root));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000); // Less than 1 second
    
    // Test children iteration performance
    start = std::chrono::high_resolution_clock::now();
    
    int childCount = 0;
    for (auto child : world.GetChildren(root)) {
        childCount++;
    }
    
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(childCount, 1000);
    EXPECT_LT(duration.count(), 100); // Should be fast
}

// Memory leak tests (basic)
TEST(MemoryTest, WorldEntityCreationDestructionTest) {
    // Test that entities are properly cleaned up
    for (int iteration = 0; iteration < 100; ++iteration) {
        SignalHandlerCollection signalHandlers;
        World world(&signalHandlers);
        
        // Create many entities
        std::vector<entity_t> entities;
        for (int i = 0; i < 100; ++i) {
            entities.push_back(world.CreateEntity());
        }
        
        // Remove half of them
        for (size_t i = 0; i < entities.size() / 2; ++i) {
            world.RemoveEntity(entities[i]);
        }
        
        // World should clean up properly when destroyed
    }
    
    // If we get here without crashes, memory management is likely working
    SUCCEED();
}

// Error handling tests
TEST(ErrorHandlingTest, WorldInvalidOperationsTest) {
    SignalHandlerCollection signalHandlers;
    World world(&signalHandlers);
    
    // These operations should not crash the program
    EXPECT_NO_THROW(world.RemoveEntity(kNullEntity));
    EXPECT_NO_THROW(world.SetParent(kNullEntity, kRoot));
    EXPECT_NO_THROW(world.GetParent(kNullEntity));
    
    // These should return safe values
    EXPECT_EQ(world.GetParent(999999), kNullEntity);
    
    // Iterators should be safe with invalid entities
    std::vector<entity_t> children;
    EXPECT_NO_THROW({
        for (auto child : world.GetChildren(999999)) {
            children.push_back(child);
        }
    });
    EXPECT_EQ(children.size(), 0);
}

// Regression tests for specific bugs (examples)
TEST(RegressionTest, EntityRemovalSiblingLinksTest) {
    // Test for a hypothetical bug where removing an entity breaks sibling links
    SignalHandlerCollection signalHandlers;
    World world(&signalHandlers);
    
    entity_t parent = world.CreateEntity();
    entity_t child1 = world.CreateEntity(parent);
    entity_t child2 = world.CreateEntity(parent);
    entity_t child3 = world.CreateEntity(parent);
    
    // Remove middle child
    world.RemoveEntity(child2);
    
    // Remaining children should still be accessible
    std::vector<entity_t> remainingChildren;
    for (auto child : world.GetChildren(parent)) {
        remainingChildren.push_back(child);
    }
    
    EXPECT_EQ(remainingChildren.size(), 2);
    EXPECT_TRUE(std::find(remainingChildren.begin(), remainingChildren.end(), child1) != remainingChildren.end());
    EXPECT_TRUE(std::find(remainingChildren.begin(), remainingChildren.end(), child3) != remainingChildren.end());
}

TEST(RegressionTest, SetParentMultipleTimesTest) {
    // Test for potential issues when setting parent multiple times
    SignalHandlerCollection signalHandlers;
    World world(&signalHandlers);
    
    entity_t entity1 = world.CreateEntity();
    entity_t entity2 = world.CreateEntity();
    entity_t target = world.CreateEntity();
    
    // Set parent multiple times
    world.SetParent(target, entity1);
    EXPECT_EQ(world.GetParent(target), entity1);
    
    world.SetParent(target, entity2);
    EXPECT_EQ(world.GetParent(target), entity2);
    
    world.SetParent(target, kRoot);
    EXPECT_EQ(world.GetParent(target), kRoot);
    
    // entity1 and entity2 should not have target as child anymore
    std::vector<entity_t> entity1Children;
    for (auto child : world.GetChildren(entity1)) {
        entity1Children.push_back(child);
    }
    EXPECT_TRUE(std::find(entity1Children.begin(), entity1Children.end(), target) == entity1Children.end());
    
    std::vector<entity_t> entity2Children;
    for (auto child : world.GetChildren(entity2)) {
        entity2Children.push_back(child);
    }
    EXPECT_TRUE(std::find(entity2Children.begin(), entity2Children.end(), target) == entity2Children.end());
}