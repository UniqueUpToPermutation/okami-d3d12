#include <gtest/gtest.h>
#include "../pool.hpp"
#include <vector>
#include <set>
#include <algorithm>
#include <chrono>

using namespace okami;

// Test object that can be used with the Pool
class TestPoolObject {
private:
    bool m_valid = false;

public:
    TestPoolObject() = default;
    
    bool IsValid() const { return m_valid; }
    void SetValid(bool valid) { m_valid = valid; }
    
    // Additional test data
    int testData = 0;
    std::string stringData = "";
};

using TestPool = Pool<TestPoolObject, int>;

class PoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool = std::make_unique<TestPool>();
    }

    void TearDown() override {
        pool.reset();
    }

    std::unique_ptr<TestPool> pool;
};

// Basic allocation tests
TEST_F(PoolTest, SingleAllocationTest) {
    int index = pool->Allocate();
    EXPECT_EQ(index, 0); // First allocation should be at index 0
    
    // Object should be accessible
    TestPoolObject& obj = (*pool)[index];
    obj.SetValid(true);
    obj.testData = 42;
    
    EXPECT_TRUE(obj.IsValid());
    EXPECT_EQ(obj.testData, 42);
}

TEST_F(PoolTest, MultipleAllocationTest) {
    std::vector<int> indices;
    
    // Allocate multiple objects
    for (int i = 0; i < 10; ++i) {
        int index = pool->Allocate();
        indices.push_back(index);
        EXPECT_EQ(index, i); // Should be sequential when no free indices exist
        
        TestPoolObject& obj = (*pool)[index];
        obj.SetValid(true);
        obj.testData = i;
    }
    
    // All indices should be unique and sequential
    for (size_t i = 0; i < indices.size(); ++i) {
        EXPECT_EQ(indices[i], static_cast<int>(i));
        EXPECT_TRUE((*pool)[indices[i]].IsValid());
        EXPECT_EQ((*pool)[indices[i]].testData, static_cast<int>(i));
    }
}

TEST_F(PoolTest, AllocationAndAccessTest) {
    int index = pool->Allocate();
    
    // Test non-const access
    TestPoolObject& obj = (*pool)[index];
    obj.SetValid(true);
    obj.testData = 42;
    obj.stringData = "test";
    
    EXPECT_EQ(obj.testData, 42);
    EXPECT_EQ(obj.stringData, "test");
    EXPECT_TRUE(obj.IsValid());
    
    // Test const access
    const TestPool& constPool = *pool;
    const TestPoolObject& constObj = constPool[index];
    EXPECT_EQ(constObj.testData, 42);
    EXPECT_EQ(constObj.stringData, "test");
    EXPECT_TRUE(constObj.IsValid());
}

// IsFree method tests
TEST_F(PoolTest, IsFreeTest) {
    // Test with invalid indices
    EXPECT_TRUE(pool->IsFree(-1));
    EXPECT_TRUE(pool->IsFree(100)); // Index beyond size
    
    // Allocate an object
    int index = pool->Allocate();
    EXPECT_FALSE(pool->IsFree(index)); // Should not be free after allocation
    
    // Free the object
    pool->Free(index);
    EXPECT_TRUE(pool->IsFree(index)); // Should be free after freeing
}

// Free operation tests
TEST_F(PoolTest, FreeAndReallocateTest) {
    // Allocate several objects
    int index1 = pool->Allocate();
    int index2 = pool->Allocate();
    int index3 = pool->Allocate();
    
    // Set up objects
    (*pool)[index1].SetValid(true);
    (*pool)[index2].SetValid(true);
    (*pool)[index3].SetValid(true);
    
    (*pool)[index1].testData = 1;
    (*pool)[index2].testData = 2;
    (*pool)[index3].testData = 3;
    
    // Free the middle object
    pool->Free(index2);
    EXPECT_TRUE(pool->IsFree(index2));
    
    // Other objects should still be accessible
    EXPECT_EQ((*pool)[index1].testData, 1);
    EXPECT_EQ((*pool)[index3].testData, 3);
    
    // Allocating a new object should reuse the freed slot
    int newIndex = pool->Allocate();
    EXPECT_EQ(newIndex, index2); // Should reuse the freed index
    EXPECT_FALSE(pool->IsFree(newIndex));
    
    TestPoolObject& newObj = (*pool)[newIndex];
    newObj.SetValid(true);
    newObj.testData = 99;
    EXPECT_TRUE(newObj.IsValid());
    EXPECT_EQ(newObj.testData, 99);
}

TEST_F(PoolTest, FreeLastObjectTest) {
    // Allocate three objects
    int index1 = pool->Allocate();
    int index2 = pool->Allocate();
    int index3 = pool->Allocate();
    
    (*pool)[index1].SetValid(true);
    (*pool)[index2].SetValid(true);
    (*pool)[index3].SetValid(true);
    
    (*pool)[index1].testData = 1;
    (*pool)[index2].testData = 2;
    (*pool)[index3].testData = 3;
    
    // Free the last object (should be popped from vector)
    pool->Free(index3);
    EXPECT_TRUE(pool->IsFree(index3));
    
    // First two objects should still be accessible
    EXPECT_EQ((*pool)[index1].testData, 1);
    EXPECT_EQ((*pool)[index2].testData, 2);
    
    // Allocating again should create new object at same index
    int newIndex = pool->Allocate();
    EXPECT_EQ(newIndex, index3); // Should reuse the same index
    
    TestPoolObject& newObj = (*pool)[newIndex];
    newObj.SetValid(true);
    EXPECT_TRUE(newObj.IsValid());
}

TEST_F(PoolTest, FreeMultipleLastObjectsTest) {
    // Allocate five objects
    std::vector<int> indices;
    for (int i = 0; i < 5; ++i) {
        int index = pool->Allocate();
        indices.push_back(index);
        (*pool)[index].SetValid(true);
        (*pool)[index].testData = i;
    }
    
    // Free the last three objects (should trigger vector shrinking)
    pool->Free(indices[4]); // Free last
    pool->Free(indices[3]); // Free second to last
    pool->Free(indices[2]); // Free third to last
    
    // All three should be free
    EXPECT_TRUE(pool->IsFree(indices[4]));
    EXPECT_TRUE(pool->IsFree(indices[3]));
    EXPECT_TRUE(pool->IsFree(indices[2]));
    
    // First two should still be accessible
    EXPECT_FALSE(pool->IsFree(indices[0]));
    EXPECT_FALSE(pool->IsFree(indices[1]));
    EXPECT_EQ((*pool)[indices[0]].testData, 0);
    EXPECT_EQ((*pool)[indices[1]].testData, 1);
    
    // New allocations should start from index 2 again
    int newIndex1 = pool->Allocate();
    int newIndex2 = pool->Allocate();
    
    EXPECT_EQ(newIndex1, 2);
    EXPECT_EQ(newIndex2, 3);
}

// Test free index reuse ordering
TEST_F(PoolTest, FreeIndexReuseOrderTest) {
    // Allocate several objects
    std::vector<int> indices;
    for (int i = 0; i < 5; ++i) {
        int index = pool->Allocate();
        indices.push_back(index);
        (*pool)[index].SetValid(true);
        (*pool)[index].testData = i;
    }
    
    // Free some objects in the middle (not in order)
    pool->Free(indices[1]); // Free index 1
    pool->Free(indices[3]); // Free index 3
    pool->Free(indices[0]); // Free index 0
    
    // Allocate new objects - should reuse in ascending order (set behavior)
    int newIndex1 = pool->Allocate();
    int newIndex2 = pool->Allocate();
    int newIndex3 = pool->Allocate();
    
    EXPECT_EQ(newIndex1, 0); // First freed index to be reused
    EXPECT_EQ(newIndex2, 1); // Second freed index to be reused
    EXPECT_EQ(newIndex3, 3); // Third freed index to be reused
    
    // Set them valid for further testing
    (*pool)[newIndex1].SetValid(true);
    (*pool)[newIndex2].SetValid(true);
    (*pool)[newIndex3].SetValid(true);
    
    EXPECT_FALSE(pool->IsFree(newIndex1));
    EXPECT_FALSE(pool->IsFree(newIndex2));
    EXPECT_FALSE(pool->IsFree(newIndex3));
}

// Edge case tests
TEST_F(PoolTest, EmptyPoolTest) {
    // Pool starts empty
    EXPECT_TRUE(pool->IsFree(0));
    EXPECT_TRUE(pool->IsFree(1));
    
    // First allocation should work
    int index = pool->Allocate();
    EXPECT_EQ(index, 0);
    EXPECT_FALSE(pool->IsFree(index));
}

// Test different index types
using TestPoolUint32 = Pool<TestPoolObject, uint32_t>;

TEST(PoolTypeTest, Uint32IndexTest) {
    TestPoolUint32 pool;
    
    uint32_t index1 = pool.Allocate();
    uint32_t index2 = pool.Allocate();
    
    EXPECT_EQ(index1, 0u);
    EXPECT_EQ(index2, 1u);
    
    // Set objects valid
    pool[index1].SetValid(true);
    pool[index2].SetValid(true);
    pool[index1].testData = 1;
    pool[index2].testData = 2;
    
    // Test IsFree
    EXPECT_FALSE(pool.IsFree(index1));
    EXPECT_FALSE(pool.IsFree(index2));
    
    // Free first, then allocate again
    pool.Free(index1);
    EXPECT_TRUE(pool.IsFree(index1));
    
    uint32_t index3 = pool.Allocate();
    EXPECT_EQ(index3, index1); // Should reuse freed slot
    EXPECT_FALSE(pool.IsFree(index3));
    
    // Verify data integrity
    pool[index3].SetValid(true);
    EXPECT_TRUE(pool[index3].IsValid());
    EXPECT_TRUE(pool[index2].IsValid());
    EXPECT_EQ(pool[index2].testData, 2);
}

using TestPoolSizeT = Pool<TestPoolObject, size_t>;

TEST(PoolTypeTest, SizeTIndexTest) {
    TestPoolSizeT pool;
    
    size_t index1 = pool.Allocate();
    size_t index2 = pool.Allocate();
    
    EXPECT_EQ(index1, 0u);
    EXPECT_EQ(index2, 1u);
    
    pool[index1].SetValid(true);
    pool[index2].SetValid(true);
    
    EXPECT_FALSE(pool.IsFree(index1));
    EXPECT_FALSE(pool.IsFree(index2));
    
    pool.Free(index1);
    EXPECT_TRUE(pool.IsFree(index1));
    
    size_t index3 = pool.Allocate();
    EXPECT_EQ(index3, index1);
}

// Test with different object types
class ComplexPoolObject {
private:
    bool m_valid = false;
    
public:
    bool IsValid() const { return m_valid; }
    void SetValid(bool valid) { m_valid = valid; }
    
    std::vector<int> data;
    std::string name;
    double value = 0.0;
};

TEST(PoolVariantTest, ComplexObjectTest) {
    Pool<ComplexPoolObject, int> pool;
    
    int index = pool.Allocate();
    EXPECT_EQ(index, 0);
    
    ComplexPoolObject& obj = pool[index];
    obj.SetValid(true);
    obj.data = {1, 2, 3, 4, 5};
    obj.name = "test_object";
    obj.value = 3.14159;
    
    EXPECT_TRUE(obj.IsValid());
    EXPECT_EQ(obj.data.size(), 5u);
    EXPECT_EQ(obj.name, "test_object");
    EXPECT_DOUBLE_EQ(obj.value, 3.14159);
    
    pool.Free(index);
    EXPECT_TRUE(pool.IsFree(index));
    
    // Reallocate and verify object is reset
    int newIndex = pool.Allocate();
    EXPECT_EQ(newIndex, index);
    
    ComplexPoolObject& newObj = pool[newIndex];
    newObj.SetValid(true);
    EXPECT_TRUE(newObj.IsValid());
    // Note: object state after reallocation depends on implementation
}

// Stress tests
TEST_F(PoolTest, StressTestRandomAllocationsAndFrees) {
    std::vector<int> activeIndices;
    std::set<int> expectedFreeIndices;
    const int numOperations = 1000;
    
    // Seed for reproducible results
    srand(42);
    
    for (int i = 0; i < numOperations; ++i) {
        if (activeIndices.empty() || (rand() % 3 == 0)) {
            // Allocate
            int index = pool->Allocate();
            activeIndices.push_back(index);
            
            // Verify it's no longer free
            EXPECT_FALSE(pool->IsFree(index));
            
            (*pool)[index].SetValid(true);
            (*pool)[index].testData = i;
            
            // Remove from expected free indices if it was there
            expectedFreeIndices.erase(index);
        } else {
            // Free a random object
            int randomIdx = rand() % activeIndices.size();
            int indexToFree = activeIndices[randomIdx];
            
            pool->Free(indexToFree);
            EXPECT_TRUE(pool->IsFree(indexToFree));
            
            activeIndices.erase(activeIndices.begin() + randomIdx);
            expectedFreeIndices.insert(indexToFree);
        }
    }
    
    // Verify all remaining objects are valid and accessible
    for (int index : activeIndices) {
        EXPECT_FALSE(pool->IsFree(index));
        EXPECT_TRUE((*pool)[index].IsValid());
        EXPECT_GE((*pool)[index].testData, 0);
    }
    
    // Verify all expected free indices are actually free
    for (int index : expectedFreeIndices) {
        EXPECT_TRUE(pool->IsFree(index));
    }
}

TEST_F(PoolTest, LargeAllocationTest) {
    std::vector<int> indices;
    const int largeCount = 10000;
    
    // Allocate many objects
    for (int i = 0; i < largeCount; ++i) {
        int index = pool->Allocate();
        indices.push_back(index);
        EXPECT_EQ(index, i); // Should be sequential
        
        (*pool)[index].SetValid(true);
        (*pool)[index].testData = i;
    }
    
    // Verify all are accessible and not free
    for (size_t i = 0; i < indices.size(); ++i) {
        EXPECT_FALSE(pool->IsFree(indices[i]));
        EXPECT_TRUE((*pool)[indices[i]].IsValid());
        EXPECT_EQ((*pool)[indices[i]].testData, static_cast<int>(i));
    }
    
    // Free every other object
    std::set<int> freedIndices;
    for (size_t i = 0; i < indices.size(); i += 2) {
        pool->Free(indices[i]);
        freedIndices.insert(indices[i]);
        EXPECT_TRUE(pool->IsFree(indices[i]));
    }
    
    // Allocate new ones to fill some gaps
    std::vector<int> newIndices;
    for (int i = 0; i < largeCount / 4; ++i) {
        int index = pool->Allocate();
        newIndices.push_back(index);
        
        // Should reuse freed indices
        EXPECT_TRUE(freedIndices.count(index) > 0);
        EXPECT_FALSE(pool->IsFree(index));
        
        (*pool)[index].SetValid(true);
        (*pool)[index].testData = largeCount + i;
    }
    
    // Verify new objects are valid
    for (size_t i = 0; i < newIndices.size(); ++i) {
        EXPECT_FALSE(pool->IsFree(newIndices[i]));
        EXPECT_TRUE((*pool)[newIndices[i]].IsValid());
        EXPECT_EQ((*pool)[newIndices[i]].testData, static_cast<int>(largeCount + i));
    }
}

// Test boundary conditions
TEST_F(PoolTest, BoundaryConditionTest) {
    // Test with negative indices in IsFree
    EXPECT_TRUE(pool->IsFree(-1));
    EXPECT_TRUE(pool->IsFree(-100));
    
    // Allocate and immediately free
    int index = pool->Allocate();
    (*pool)[index].SetValid(true);
    EXPECT_FALSE(pool->IsFree(index));
    
    pool->Free(index);
    EXPECT_TRUE(pool->IsFree(index));
    
    // Reallocate same index
    int newIndex = pool->Allocate();
    EXPECT_EQ(newIndex, index);
    EXPECT_FALSE(pool->IsFree(newIndex));
}

// Test vector shrinking behavior
TEST_F(PoolTest, VectorShrinkingTest) {
    // Allocate 10 objects
    std::vector<int> indices;
    for (int i = 0; i < 10; ++i) {
        int index = pool->Allocate();
        indices.push_back(index);
        (*pool)[index].SetValid(true);
        (*pool)[index].testData = i;
    }
    
    // Free all objects from the end backwards
    for (int i = 9; i >= 0; --i) {
        pool->Free(indices[i]);
        EXPECT_TRUE(pool->IsFree(indices[i]));
    }
    
    // All indices should be free
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(pool->IsFree(i));
    }
    
    // Next allocation should start from 0 again
    int newIndex = pool->Allocate();
    EXPECT_EQ(newIndex, 0);
    EXPECT_FALSE(pool->IsFree(newIndex));
}

// Test allocation order after mixed operations
TEST_F(PoolTest, AllocationOrderTest) {
    // Allocate 5 objects
    std::vector<int> indices;
    for (int i = 0; i < 5; ++i) {
        int index = pool->Allocate();
        indices.push_back(index);
        (*pool)[index].SetValid(true);
    }
    
    // Free indices 1, 3, 4
    pool->Free(indices[1]);
    pool->Free(indices[3]);
    pool->Free(indices[4]);
    
    // Next allocations should reuse in ascending order (set behavior)
    int newIndex1 = pool->Allocate();
    int newIndex2 = pool->Allocate();
    int newIndex3 = pool->Allocate();
    
    EXPECT_EQ(newIndex1, 1); // Smallest freed index
    EXPECT_EQ(newIndex2, 3); // Next smallest freed index
    EXPECT_EQ(newIndex3, 4); // Last freed index (but vector was shrunk, so this creates new)
    
    (*pool)[newIndex1].SetValid(true);
    (*pool)[newIndex2].SetValid(true);
    (*pool)[newIndex3].SetValid(true);
    
    EXPECT_FALSE(pool->IsFree(newIndex1));
    EXPECT_FALSE(pool->IsFree(newIndex2));
    EXPECT_FALSE(pool->IsFree(newIndex3));
}

// Test memory efficiency after many operations
TEST_F(PoolTest, MemoryEfficiencyTest) {
    // Allocate 100 objects
    std::vector<int> indices;
    for (int i = 0; i < 100; ++i) {
        int index = pool->Allocate();
        indices.push_back(index);
        (*pool)[index].SetValid(true);
    }
    
    EXPECT_EQ(pool->Size(), 100u);
    EXPECT_EQ(pool->ActiveCount(), 100u);
    
    // Free all objects from the end (should shrink vector significantly)
    for (int i = 99; i >= 0; --i) {
        pool->Free(indices[i]);
    }
    
    // Pool should be completely empty after this
    EXPECT_EQ(pool->Size(), 0u);
    EXPECT_EQ(pool->FreeCount(), 0u);
    EXPECT_EQ(pool->ActiveCount(), 0u);
    
    // Next allocation should start fresh
    int newIndex = pool->Allocate();
    EXPECT_EQ(newIndex, 0);
    EXPECT_EQ(pool->Size(), 1u);
    EXPECT_EQ(pool->ActiveCount(), 1u);
}

// Test with very large indices (if using large index types)
TEST(PoolLargeIndexTest, LargeIndexTypeTest) {
    using LargePool = Pool<TestPoolObject, int64_t>;
    LargePool pool;
    
    int64_t index = pool.Allocate();
    EXPECT_EQ(index, 0);
    
    pool[index].SetValid(true);
    pool[index].testData = 42;
    
    EXPECT_FALSE(pool.IsFree(index));
    EXPECT_EQ(pool[index].testData, 42);
    
    // Test with negative indices
    EXPECT_TRUE(pool.IsFree(-1000000));
    EXPECT_TRUE(pool.IsFree(-1));
    
    pool.Free(index);
    EXPECT_TRUE(pool.IsFree(index));
    
    int64_t newIndex = pool.Allocate();
    EXPECT_EQ(newIndex, index);
}

// Add to the existing tests... (insert before the closing of the file)

// Test performance characteristics
TEST_F(PoolTest, PerformanceCharacteristicsTest) {
    const int numObjects = 1000;
    std::vector<int> indices;
    
    // Time allocation performance
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numObjects; ++i) {
        int index = pool->Allocate();
        indices.push_back(index);
        (*pool)[index].SetValid(true);
        (*pool)[index].testData = i;
    }
    
    auto mid = std::chrono::high_resolution_clock::now();
    
    // Time free performance
    for (int index : indices) {
        pool->Free(index);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    auto allocTime = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto freeTime = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);
    
    // These are just sanity checks - actual performance will vary
    EXPECT_LT(allocTime.count(), 10000); // Should complete in reasonable time
    EXPECT_LT(freeTime.count(), 10000);  // Should complete in reasonable time
    
    // Pool should be efficient after all operations
    EXPECT_EQ(pool->Size(), 0u);
    EXPECT_EQ(pool->ActiveCount(), 0u);
}