#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include "../entity_tree.hpp"
#include "../engine.hpp"

using namespace okami;

// Benchmark utilities
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double ElapsedMilliseconds() const {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        return duration.count() / 1000.0;
    }
    
    void Reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

// Benchmark tests
class WorldBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
		signalHandlers = std::make_unique<SignalHandlerCollection>();
        world = std::make_unique<EntityTree>();
        rng.seed(42); // Fixed seed for reproducible results
    }

    void TearDown() override {
        world.reset();
    }

    std::unique_ptr<EntityTree> world;
	std::unique_ptr<SignalHandlerCollection> signalHandlers;
    std::mt19937 rng;
};

TEST_F(WorldBenchmark, EntityCreationBenchmark) {
    Timer timer;
    
    const int numEntities = 10000;
    std::vector<entity_t> entities;
    entities.reserve(numEntities);
    
    // Benchmark entity creation
    timer.Reset();
	world->BeginUpdates(*signalHandlers);
    for (int i = 0; i < numEntities; ++i) {
        entities.push_back(world->CreateEntity());
    }
    double creationTime = timer.ElapsedMilliseconds();
    
    std::cout << "Created " << numEntities << " entities in " 
              << creationTime << "ms (" 
              << (creationTime * 1000.0 / numEntities) << " us per entity)" << std::endl;
    
    // Benchmark should complete in reasonable time
    EXPECT_LT(creationTime, 1000.0); // Less than 1 second
    
    // Benchmark entity removal
    timer.Reset();
    for (entity_t entity : entities) {
        world->RemoveEntity(entity);
    }
    world->EndUpdates();
    double removalTime = timer.ElapsedMilliseconds();
    
    std::cout << "Removed " << numEntities << " entities in " 
              << removalTime << "ms (" 
              << (removalTime * 1000.0 / numEntities) << " us per entity)" << std::endl;
    
    EXPECT_LT(removalTime, 1000.0); // Less than 1 second
}

TEST_F(WorldBenchmark, HierarchyTraversalBenchmark) {
    // Create a balanced tree: root with 100 children, each with 100 children
    const int numBranches = 100;
    const int numLeaves = 100;
    
    Timer timer;
    
    // Create hierarchy
    std::vector<entity_t> branches;
    std::vector<entity_t> leaves;
    
	world->BeginUpdates(*signalHandlers);
    for (int i = 0; i < numBranches; ++i) {
        entity_t branch = world->CreateEntity();
        branches.push_back(branch);
        
        for (int j = 0; j < numLeaves; ++j) {
            leaves.push_back(world->CreateEntity(branch));
        }
    }
    world->EndUpdates();
    
    // Benchmark children iteration
    timer.Reset();
    int totalChildren = 0;
    for (entity_t branch : branches) {
        for (auto child : world->GetChildren(branch)) {
            totalChildren++;
            (void)child; // Suppress unused variable warning
        }
    }
    double childrenTime = timer.ElapsedMilliseconds();
    
    std::cout << "Iterated over " << totalChildren << " children in " 
              << childrenTime << "ms" << std::endl;
    
    EXPECT_EQ(totalChildren, numBranches * numLeaves);
    EXPECT_LT(childrenTime, 100.0); // Should be fast
    
    // Benchmark ancestor traversal
    timer.Reset();
    int totalAncestors = 0;
    for (entity_t leaf : leaves) {
        for (auto ancestor : world->GetAncestors(leaf)) {
            totalAncestors++;
            (void)ancestor; // Suppress unused variable warning
        }
    }
    double ancestorTime = timer.ElapsedMilliseconds();
    
    std::cout << "Traversed " << totalAncestors << " ancestors in " 
              << ancestorTime << "ms" << std::endl;
    
    // Each leaf should have 2 ancestors (branch + root)
    EXPECT_EQ(totalAncestors, leaves.size() * 2);
    EXPECT_LT(ancestorTime, 100.0); // Should be fast
}

// Memory usage test (basic)
TEST_F(WorldBenchmark, MemoryUsageBenchmark) {
    const int numEntities = 50000;
    
    Timer timer;
    
    // Create many entities
    std::vector<entity_t> entities;
    entities.reserve(numEntities);
    
	world->BeginUpdates(*signalHandlers);
    for (int i = 0; i < numEntities; ++i) {
        entities.push_back(world->CreateEntity());
    }
    world->EndUpdates();
    
    double creationTime = timer.ElapsedMilliseconds();
    
    std::cout << "Created " << numEntities << " entities in " 
              << creationTime << "ms" << std::endl;
    
    // Test that we can create a large number of entities
    EXPECT_EQ(entities.size(), numEntities);
    EXPECT_LT(creationTime, 5000.0); // Should complete within 5 seconds
    
    // Clean up
    timer.Reset();
    world.reset();
    double cleanupTime = timer.ElapsedMilliseconds();
    
    std::cout << "Cleanup took " << cleanupTime << "ms" << std::endl;
    EXPECT_LT(cleanupTime, 1000.0); // Cleanup should be fast
}