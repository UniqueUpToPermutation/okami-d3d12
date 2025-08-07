#include <gtest/gtest.h>
#include "../aabb_tree.hpp"
#include <glm/vec3.hpp>
#include <vector>
#include <random>
#include <chrono>

using namespace okami;

// Test fixture for AABB Tree tests
class AABBTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        tree = std::make_unique<AABBTree<int>>();
        rng.seed(42); // Fixed seed for reproducible results
    }

    void TearDown() override {
        tree.reset();
    }

    std::unique_ptr<AABBTree<int>> tree;
    std::mt19937 rng;

    // Helper function to create AABB
    AABB CreateAABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
        return AABB{
            glm::vec3(minX, minY, minZ),
            glm::vec3(maxX, maxY, maxZ)
        };
    }

    // Helper function to create unit AABB at position
    AABB CreateUnitAABB(float x, float y, float z) {
        return CreateAABB(x, y, z, x + 1.0f, y + 1.0f, z + 1.0f);
    }
};

// AABB utility function tests
TEST_F(AABBTreeTest, AABBContainsPointTest) {
    AABB box = CreateAABB(0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f);
    
    EXPECT_TRUE(box.Contains(glm::vec3(1.0f, 1.0f, 1.0f)));
    EXPECT_TRUE(box.Contains(glm::vec3(0.0f, 0.0f, 0.0f))); // Edge case: boundary
    EXPECT_TRUE(box.Contains(glm::vec3(2.0f, 2.0f, 2.0f))); // Edge case: boundary
    EXPECT_FALSE(box.Contains(glm::vec3(-1.0f, 1.0f, 1.0f)));
    EXPECT_FALSE(box.Contains(glm::vec3(3.0f, 1.0f, 1.0f)));
}

TEST_F(AABBTreeTest, AABBContainsAABBTest) {
    AABB outer = CreateAABB(0.0f, 0.0f, 0.0f, 4.0f, 4.0f, 4.0f);
    AABB inner = CreateAABB(1.0f, 1.0f, 1.0f, 3.0f, 3.0f, 3.0f);
    AABB overlapping = CreateAABB(2.0f, 2.0f, 2.0f, 6.0f, 6.0f, 6.0f);
    AABB separate = CreateAABB(5.0f, 5.0f, 5.0f, 7.0f, 7.0f, 7.0f);
    
    EXPECT_TRUE(outer.Contains(inner));
    EXPECT_FALSE(inner.Contains(outer));
    EXPECT_FALSE(outer.Contains(overlapping));
    EXPECT_FALSE(outer.Contains(separate));
}

TEST_F(AABBTreeTest, AABBUnionTest) {
    AABB a = CreateAABB(0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f);
    AABB b = CreateAABB(1.0f, 1.0f, 1.0f, 3.0f, 3.0f, 3.0f);
    
    AABB result = Union(a, b);
    
    EXPECT_EQ(result.m_min.x, 0.0f);
    EXPECT_EQ(result.m_min.y, 0.0f);
    EXPECT_EQ(result.m_min.z, 0.0f);
    EXPECT_EQ(result.m_max.x, 3.0f);
    EXPECT_EQ(result.m_max.y, 3.0f);
    EXPECT_EQ(result.m_max.z, 3.0f);
}

TEST_F(AABBTreeTest, AABBIntersectionTest) {
    AABB a = CreateAABB(0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f);
    AABB b = CreateAABB(1.0f, 1.0f, 1.0f, 3.0f, 3.0f, 3.0f);
    
    AABB result = Intersection(a, b);
    
    EXPECT_EQ(result.m_min.x, 1.0f);
    EXPECT_EQ(result.m_min.y, 1.0f);
    EXPECT_EQ(result.m_min.z, 1.0f);
    EXPECT_EQ(result.m_max.x, 2.0f);
    EXPECT_EQ(result.m_max.y, 2.0f);
    EXPECT_EQ(result.m_max.z, 2.0f);
}

TEST_F(AABBTreeTest, AABBIntersectsTest) {
    AABB a = CreateAABB(0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f);
    AABB b = CreateAABB(1.0f, 1.0f, 1.0f, 3.0f, 3.0f, 3.0f);
    AABB c = CreateAABB(4.0f, 4.0f, 4.0f, 5.0f, 5.0f, 5.0f);
    
    EXPECT_TRUE(Intersects(a, b));
    EXPECT_FALSE(Intersects(a, c));
    EXPECT_FALSE(Intersects(b, c));

}

TEST_F(AABBTreeTest, AABBVolumeTest) {
    AABB box = CreateAABB(0.0f, 0.0f, 0.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(Volume(box), 24.0f); // 2 * 3 * 4 = 24
}

TEST_F(AABBTreeTest, AABBSurfaceAreaTest) {
    AABB box = CreateAABB(0.0f, 0.0f, 0.0f, 2.0f, 3.0f, 4.0f);
    // Surface area = 2 * (2*3 + 3*4 + 4*2) = 2 * (6 + 12 + 8) = 52
    EXPECT_FLOAT_EQ(SurfaceArea(box), 52.0f);
}

// Basic tree operations tests
TEST_F(AABBTreeTest, EmptyTreeTest) {
    EXPECT_TRUE(tree->Validate());
}

TEST_F(AABBTreeTest, SingleNodeInsertTest) {
    AABB box = CreateUnitAABB(0.0f, 0.0f, 0.0f);
    int nodeIndex = tree->Insert(box, 42);
    
    EXPECT_TRUE(tree->Validate());
    EXPECT_NE(nodeIndex, kInvalidNodeIndex);
}

TEST_F(AABBTreeTest, MultipleNodeInsertTest) {
    std::vector<int> nodeIndices;
    
    for (int i = 0; i < 10; ++i) {
        AABB box = CreateUnitAABB(static_cast<float>(i), 0.0f, 0.0f);
        int nodeIndex = tree->Insert(box, i);
        nodeIndices.push_back(nodeIndex);
        
        EXPECT_NE(nodeIndex, kInvalidNodeIndex);
        EXPECT_TRUE(tree->Validate());
    }
    
    // All node indices should be unique
    std::sort(nodeIndices.begin(), nodeIndices.end());
    auto it = std::unique(nodeIndices.begin(), nodeIndices.end());
    EXPECT_EQ(it, nodeIndices.end());
}

TEST_F(AABBTreeTest, RemoveNodeTest) {
    AABB box1 = CreateUnitAABB(0.0f, 0.0f, 0.0f);
    AABB box2 = CreateUnitAABB(2.0f, 0.0f, 0.0f);
    
    int node1 = tree->Insert(box1, 1);
    int node2 = tree->Insert(box2, 2);
    
    EXPECT_TRUE(tree->Validate());
    
    tree->Remove(node1);
    EXPECT_TRUE(tree->Validate());
    
    tree->Remove(node2);
    EXPECT_TRUE(tree->Validate());
}

TEST_F(AABBTreeTest, RemoveFromSingleNodeTreeTest) {
    AABB box = CreateUnitAABB(0.0f, 0.0f, 0.0f);
    int nodeIndex = tree->Insert(box, 42);
    
    EXPECT_TRUE(tree->Validate());
    
    tree->Remove(nodeIndex);
    EXPECT_TRUE(tree->Validate());
}

TEST_F(AABBTreeTest, RemoveNonLeafNodeTest) {
    // This should test the error handling for removing non-leaf nodes
    // Note: This test might need to be adapted based on the actual implementation
    AABB box1 = CreateUnitAABB(0.0f, 0.0f, 0.0f);
    AABB box2 = CreateUnitAABB(2.0f, 0.0f, 0.0f);
    
    int node1 = tree->Insert(box1, 1);
    int node2 = tree->Insert(box2, 2);
    
    // In the current implementation, we can't directly access internal nodes
    // This test would need modification based on the actual API
}

TEST_F(AABBTreeTest, ClearTreeTest) {
    // Insert several nodes
    for (int i = 0; i < 5; ++i) {
        AABB box = CreateUnitAABB(static_cast<float>(i), 0.0f, 0.0f);
        tree->Insert(box, i);
    }
    
    EXPECT_TRUE(tree->Validate());
    
    tree->Clear();
    EXPECT_TRUE(tree->Validate());
}

// Stress tests
TEST_F(AABBTreeTest, LargeTreeInsertTest) {
    const int numNodes = 1000;
    std::vector<int> nodeIndices;
    
    for (int i = 0; i < numNodes; ++i) {
        float x = static_cast<float>(i % 32);
        float y = static_cast<float>((i / 32) % 32);
        float z = static_cast<float>(i / (32 * 32));
        
        AABB box = CreateUnitAABB(x, y, z);
        int nodeIndex = tree->Insert(box, i);
        nodeIndices.push_back(nodeIndex);
    }
    
    EXPECT_TRUE(tree->Validate());
    
    // Remove half the nodes
    for (int i = 0; i < numNodes / 2; ++i) {
        tree->Remove(nodeIndices[i * 2]);
    }
    
    EXPECT_TRUE(tree->Validate());
}

TEST_F(AABBTreeTest, RandomInsertRemoveTest) {
    const int numOperations = 500;
    std::vector<int> activeNodes;
    std::uniform_real_distribution<float> posDist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> sizeDist(0.1f, 2.0f);
    std::uniform_int_distribution<int> opDist(0, 1);
    
    for (int i = 0; i < numOperations; ++i) {
        if (activeNodes.empty() || opDist(rng) == 0) {
            // Insert operation
            float x = posDist(rng);
            float y = posDist(rng);
            float z = posDist(rng);
            float m_size = sizeDist(rng);
            
            AABB box = CreateAABB(x, y, z, x + m_size, y + m_size, z + m_size);
            int nodeIndex = tree->Insert(box, i);
            activeNodes.push_back(nodeIndex);
        } else {
            // Remove operation
            std::uniform_int_distribution<size_t> indexDist(0, activeNodes.size() - 1);
            size_t removeIndex = indexDist(rng);
            
            tree->Remove(activeNodes[removeIndex]);
            activeNodes.erase(activeNodes.begin() + removeIndex);
        }
        
        EXPECT_TRUE(tree->Validate()) << "Tree validation failed at operation " << i;
    }
}

// Custom cost function test
struct LinearCostFunction {
    inline float operator()(const AABB& aabb) const {
        // Simple linear cost based on diagonal length
        glm::vec3 m_size = aabb.m_max - aabb.m_min;
        return m_size.x + m_size.y + m_size.z;
    }
};

TEST_F(AABBTreeTest, CustomCostFunctionTest) {
    AABBTree<int, LinearCostFunction> customTree;
    
    for (int i = 0; i < 10; ++i) {
        AABB box = CreateUnitAABB(static_cast<float>(i), 0.0f, 0.0f);
        int nodeIndex = customTree.Insert(box, i);
        EXPECT_NE(nodeIndex, kInvalidNodeIndex);
        EXPECT_TRUE(customTree.Validate());
    }
}

// Performance benchmark test
TEST_F(AABBTreeTest, PerformanceBenchmark) {
    const int numInserts = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numInserts; ++i) {
        float x = static_cast<float>(i % 100);
        float y = static_cast<float>((i / 100) % 100);
        float z = static_cast<float>(i / 10000);
        
        AABB box = CreateUnitAABB(x, y, z);
        tree->Insert(box, i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Inserted " << numInserts << " nodes in " << duration.count() << "ms" << std::endl;
    
    // Should complete in reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 5000); // Less than 5 seconds
    EXPECT_TRUE(tree->Validate());
}

// Edge cases
TEST_F(AABBTreeTest, ZeroSizeAABBTest) {
    AABB zeroBox = CreateAABB(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    int nodeIndex = tree->Insert(zeroBox, 42);
    
    EXPECT_NE(nodeIndex, kInvalidNodeIndex);
    EXPECT_TRUE(tree->Validate());
}

TEST_F(AABBTreeTest, NegativeCoordinatesTest) {
    AABB negBox = CreateAABB(-5.0f, -5.0f, -5.0f, -3.0f, -3.0f, -3.0f);
    int nodeIndex = tree->Insert(negBox, 42);
    
    EXPECT_NE(nodeIndex, kInvalidNodeIndex);
    EXPECT_TRUE(tree->Validate());
}

TEST_F(AABBTreeTest, LargeAABBTest) {
    AABB largeBox = CreateAABB(-1000.0f, -1000.0f, -1000.0f, 1000.0f, 1000.0f, 1000.0f);
    int nodeIndex = tree->Insert(largeBox, 42);
    
    EXPECT_NE(nodeIndex, kInvalidNodeIndex);
    EXPECT_TRUE(tree->Validate());
}

// Test with different data types
TEST_F(AABBTreeTest, DifferentDataTypesTest) {
    AABBTree<std::string> stringTree;
    AABB box = CreateUnitAABB(0.0f, 0.0f, 0.0f);
    int nodeIndex = stringTree.Insert(box, "test_data");
    
    EXPECT_NE(nodeIndex, kInvalidNodeIndex);
    EXPECT_TRUE(stringTree.Validate());
}

// Balanced tree tests
TEST_F(AABBTreeTest, BalancedInsertionTest) {
    // Insert nodes in a pattern that should trigger balancing
    const int gridSize = 8;
    
    for (int x = 0; x < gridSize; ++x) {
        for (int y = 0; y < gridSize; ++y) {
            for (int z = 0; z < gridSize; ++z) {
                AABB box = CreateUnitAABB(
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(z)
                );
                int nodeIndex = tree->Insert(box, x * gridSize * gridSize + y * gridSize + z);
                EXPECT_NE(nodeIndex, kInvalidNodeIndex);
            }
        }
    }
    
    EXPECT_TRUE(tree->Validate());
    
    // The tree should be reasonably balanced
    // This is hard to test without exposing internal structure,
    // but validation should pass
}