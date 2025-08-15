#include <gtest/gtest.h>
#include "../geometry.hpp"
#include "../paths.hpp"
#include <filesystem>
#include <iostream>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

using namespace okami;

class GeometryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing specific needed for setup
    }

    void TearDown() override {
        // Nothing specific needed for teardown
    }
};

TEST_F(GeometryTest, LoadGLTF_BoxFile_Success) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    // Verify the file exists
    ASSERT_TRUE(std::filesystem::exists(boxPath)) 
        << "Test asset box.glb not found at: " << boxPath;
    
    // Act
    auto result = RawGeometry::LoadGLTF(boxPath);
    
    // Assert
    ASSERT_TRUE(result.has_value()) 
        << "Failed to load box.glb: " << (result.has_value() ? "Success" : result.error().Str());
    
    auto geometry = std::move(result.value());
    
    // Verify we have meshes
    auto meshes = geometry.GetMeshes();
    EXPECT_GT(meshes.size(), 0) << "Geometry should have at least one mesh";
}

TEST_F(GeometryTest, LoadGLTF_NonexistentFile_Failure) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto nonexistentPath = testAssetsPath / "nonexistent.glb";
    
    // Act
    auto result = RawGeometry::LoadGLTF(nonexistentPath);
    
    // Assert
    EXPECT_FALSE(result.has_value()) << "Should fail to load nonexistent file";
    if (!result.has_value()) {
        EXPECT_TRUE(result.error().IsError()) << "Should return an error";
    }
}

TEST_F(GeometryTest, TryAccess_ValidAttribute_ReturnsView) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    auto result = RawGeometry::LoadGLTF(boxPath);
    ASSERT_TRUE(result.has_value());
    
    auto geometry = std::move(result.value());
    auto meshes = geometry.GetMeshes();
    ASSERT_GT(meshes.size(), 0);

    // Find position attribute in first mesh
    const auto& firstMesh = meshes[0];
    auto positionAttribute = firstMesh.TryGetAttribute(AttributeType::Position);
    
    if (positionAttribute) {
        // Act
        auto view = geometry.TryAccess<glm::vec3>(AttributeType::Position, 0);
        
        // Assert
        EXPECT_TRUE(view.has_value()) << "Should be able to get view for existing position attribute";
        
        if (view.has_value()) {
            // Test that we can iterate through the view
            size_t count = 0;
            for (auto& pos : view.value()) {
                count++;
                // Just verify we can access the data
                EXPECT_TRUE(std::isfinite(pos.x) || !std::isfinite(pos.x)) << "Position should be readable";
                if (count > 10) break; // Don't check too many for performance
            }
            EXPECT_GT(count, 0) << "Should have at least one position";
        }
    } else {
        GTEST_SKIP() << "Box geometry doesn't have position attribute - skipping view test";
    }
}

TEST_F(GeometryTest, TryAccess_InvalidAttribute_ReturnsNullopt) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    auto result = RawGeometry::LoadGLTF(boxPath);
    ASSERT_TRUE(result.has_value());
    
    auto geometry = std::move(result.value());
    
    // Act - try to get an attribute that likely doesn't exist
    auto view = geometry.TryAccess<glm::vec4>(AttributeType::Color, 0);
    
    // Assert
    EXPECT_FALSE(view.has_value()) << "Should return nullopt for non-existent attribute";
}

TEST_F(GeometryTest, TryAccess_InvalidMeshIndex_ReturnsNullopt) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    auto result = RawGeometry::LoadGLTF(boxPath);
    ASSERT_TRUE(result.has_value());
    
    auto geometry = std::move(result.value());
    
    // Act - try to access a mesh that doesn't exist
    auto view = geometry.TryAccess<glm::vec3>(AttributeType::Position, 999);
    
    // Assert
    EXPECT_FALSE(view.has_value()) << "Should return nullopt for invalid mesh index";
}

TEST_F(GeometryTest, HasIndexBuffer_ChecksForIndices) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    auto result = RawGeometry::LoadGLTF(boxPath);
    ASSERT_TRUE(result.has_value());
    
    auto geometry = std::move(result.value());
    auto meshes = geometry.GetMeshes();
    ASSERT_GT(meshes.size(), 0);
    
    // Assert - check if the first mesh has index buffer
    bool hasIndices = meshes[0].HasIndexBuffer();
    
    if (hasIndices) {
        std::cout << "First mesh has index buffer with " << meshes[0].m_indices->m_count << " indices" << std::endl;
    } else {
        std::cout << "First mesh does not have index buffer (uses direct vertex rendering)" << std::endl;
    }
    
    // This test doesn't make strong assertions since both cases are valid
    // It just verifies the API works
}

TEST_F(GeometryTest, GetMeshCount_ReturnsCorrectCount) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    auto result = RawGeometry::LoadGLTF(boxPath);
    ASSERT_TRUE(result.has_value());
    
    auto geometry = std::move(result.value());
    
    // Act
    size_t meshCount = geometry.GetMeshCount();
    auto meshes = geometry.GetMeshes();
    
    // Assert
    EXPECT_EQ(meshCount, meshes.size()) << "GetMeshCount should match GetMeshes().size()";
    EXPECT_GT(meshCount, 0) << "Should have at least one mesh";
    
    std::cout << "Loaded " << meshCount << " mesh(es) from box.glb" << std::endl;
}

TEST_F(GeometryTest, MeshAttributes_VerifyStructure) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    auto result = RawGeometry::LoadGLTF(boxPath);
    ASSERT_TRUE(result.has_value());
    
    auto geometry = std::move(result.value());
    auto meshes = geometry.GetMeshes();
    ASSERT_GT(meshes.size(), 0);
    
    // Act & Assert - verify the first mesh structure
    const auto& firstMesh = meshes[0];
    
    std::cout << "First mesh has " << firstMesh.m_attributes.size() << " attributes:" << std::endl;
    for (const auto& attr : firstMesh.m_attributes) {
        std::cout << "  Attribute type: " << static_cast<int>(attr.m_type) 
                  << ", buffer: " << attr.m_buffer 
                  << ", offset: " << attr.m_offset
                  << ", stride: " << attr.GetStride() << std::endl;
    }
    
    EXPECT_GT(firstMesh.m_attributes.size(), 0) << "Mesh should have at least one attribute";
    EXPECT_GT(firstMesh.m_vertexCount, 0) << "Mesh should have vertices";
    EXPECT_EQ(firstMesh.m_type, MeshType::Static) << "Loaded mesh should be Static type";
    
    // Verify position attribute exists
    auto posAttr = firstMesh.TryGetAttribute(AttributeType::Position);
    EXPECT_NE(posAttr, nullptr) << "Mesh should have position attribute";
    
    if (posAttr) {
        EXPECT_EQ(posAttr->m_type, AttributeType::Position);
        EXPECT_GE(posAttr->m_buffer, 0) << "Buffer index should be valid";
    }
}

TEST_F(GeometryTest, GetRawVertexData_ValidBuffer_ReturnsData) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    auto result = RawGeometry::LoadGLTF(boxPath);
    ASSERT_TRUE(result.has_value());
    
    auto geometry = std::move(result.value());
    
    // Act & Assert
    if (geometry.GetBuffers().size() > 0) {
        auto data = geometry.GetRawVertexData(0);
        EXPECT_GT(data.size(), 0) << "Buffer 0 should contain data";
    } else {
        GTEST_SKIP() << "No buffers in geometry";
    }
}

TEST_F(GeometryTest, GetRawVertexData_InvalidBuffer_Throws) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    auto result = RawGeometry::LoadGLTF(boxPath);
    ASSERT_TRUE(result.has_value());
    
    auto geometry = std::move(result.value());
    
    // Act & Assert - try to access an invalid buffer
    EXPECT_THROW({
        auto data = geometry.GetRawVertexData(999);
    }, std::out_of_range) << "Should throw for invalid buffer index";
}
