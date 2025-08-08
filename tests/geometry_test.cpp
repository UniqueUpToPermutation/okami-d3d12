#include <gtest/gtest.h>
#include "../geometry.hpp"
#include "../paths.hpp"
#include <filesystem>
#include <iostream>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

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
    
    // Define attributes we want to load (interleaved in single buffer)
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 24},   // glm::vec3 at offset 0
        {AttributeType::Normal, 0, 12, 12, 24}     // glm::vec3 at offset 12
    };
    
    // Act
    auto result = Geometry::LoadGLTF(boxPath, requestedAttributes);
    
    // Assert
    ASSERT_TRUE(result.has_value()) 
        << "Failed to load box.glb: " << (result.error().IsError() ? result.error().Str() : "Unknown error");
    
    const auto& geometry = result.value();
    
    // Verify we have some attributes
    auto attributes = geometry.GetAttributes();
    EXPECT_GT(attributes.size(), 0) << "Geometry should have at least one attribute";
    
    // Verify we have vertex data
    auto vertexData = geometry.GetRawVertexData();
    EXPECT_GT(vertexData.size(), 0) << "Geometry should have vertex data";
    
    // Check for expected attributes (a basic box should have at least position)
    bool hasPosition = false;
    bool hasNormal = false;
    bool hasTexCoord = false;
    
    std::cout << "Found " << attributes.size() << " attributes:" << std::endl;
    
    for (const auto& attr : attributes) {
        std::cout << "  Attribute type: " << static_cast<int>(attr.m_type) 
                  << ", size: " << attr.m_size 
                  << ", offset: " << attr.m_offset 
                  << ", stride: " << attr.m_stride 
                  << ", buffer: " << attr.m_bufferIndex << std::endl;
                  
        switch (attr.m_type) {
            case AttributeType::Position:
                hasPosition = true;
                std::cout << "    -> Position attribute found with size " << attr.m_size << " bytes" << std::endl;
                // Don't require specific size since we don't know the data type
                break;
            case AttributeType::Normal:
                hasNormal = true;
                std::cout << "    -> Normal attribute found with size " << attr.m_size << " bytes" << std::endl;
                break;
            case AttributeType::TexCoord:
                hasTexCoord = true;
                std::cout << "    -> TexCoord attribute found with size " << attr.m_size << " bytes" << std::endl;
                break;
            default:
                std::cout << "    -> Unknown attribute type" << std::endl;
                break;
        }
    }
    
    EXPECT_TRUE(hasPosition) << "Box geometry should have position attribute";
    
    // If we have position data, verify we can access it
    if (hasPosition) {
        try {
            auto positionView = geometry.GetConstView<glm::vec3>(AttributeType::Position);
            // We should be able to get the view without throwing
            SUCCEED() << "Successfully created position view";
        } catch (const std::exception& e) {
            FAIL() << "Failed to create position view: " << e.what();
        }
    }
}

TEST_F(GeometryTest, LoadGLTF_NonexistentFile_Failure) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto nonexistentPath = testAssetsPath / "nonexistent.glb";
    
    // Define some basic attributes
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 12}
    };
    
    // Act
    auto result = Geometry::LoadGLTF(nonexistentPath, requestedAttributes);
    
    // Assert
    EXPECT_FALSE(result.has_value()) << "Should fail to load nonexistent file";
    if (!result.has_value()) {
        EXPECT_TRUE(result.error().IsError()) << "Should return an error";
    }
}

TEST_F(GeometryTest, LoadGLTF_UnsupportedExtension_Failure) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto invalidPath = testAssetsPath / "test.txt";
    
    // Define some basic attributes
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 12}
    };
    
    // Act
    auto result = Geometry::LoadGLTF(invalidPath, requestedAttributes);
    
    // Assert
    EXPECT_FALSE(result.has_value()) << "Should fail to load file with unsupported extension";
    if (!result.has_value()) {
        EXPECT_TRUE(result.error().IsError()) << "Should return an error";
        EXPECT_NE(result.error().Str().find("Unsupported file format"), std::string::npos) 
            << "Error message should mention unsupported file format";
    }
}

TEST_F(GeometryTest, GetView_ValidAttribute_Success) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    // Define attributes we want to load
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 24},   // glm::vec3 at offset 0
        {AttributeType::Normal, 0, 12, 12, 24}     // glm::vec3 at offset 12
    };
    
    auto result = Geometry::LoadGLTF(boxPath, requestedAttributes);
    ASSERT_TRUE(result.has_value());
    
    const auto& geometry = result.value();
    auto attributes = geometry.GetAttributes();
    
    // Find position attribute
    bool hasPosition = false;
    for (const auto& attr : attributes) {
        if (attr.m_type == AttributeType::Position) {
            hasPosition = true;
            break;
        }
    }
    
    if (hasPosition) {
        // Act & Assert
        EXPECT_NO_THROW({
            auto view = geometry.GetConstView<glm::vec3>(AttributeType::Position);
        }) << "Should be able to create view for existing position attribute";
    } else {
        GTEST_SKIP() << "Box geometry doesn't have position attribute - skipping view test";
    }
}

TEST_F(GeometryTest, GetView_InvalidAttribute_Throws) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    // Define attributes we want to load (only position, no color)
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 12}
    };
    
    auto result = Geometry::LoadGLTF(boxPath, requestedAttributes);
    ASSERT_TRUE(result.has_value());
    
    const auto& geometry = result.value();
    
    // Act & Assert - try to get an attribute that we didn't load
    EXPECT_THROW({
        auto view = geometry.GetConstView<float>(AttributeType::Color);
    }, std::runtime_error) << "Should throw when requesting non-existent attribute";
}

TEST_F(GeometryTest, LoadGLTF_IndexBuffer_LoadedIfAvailable) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    // Define attributes we want to load
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 24},   // glm::vec3 at offset 0
        {AttributeType::Normal, 0, 12, 12, 24}     // glm::vec3 at offset 12
    };
    
    // Act
    auto result = Geometry::LoadGLTF(boxPath, requestedAttributes);
    ASSERT_TRUE(result.has_value());
    
    const auto& geometry = result.value();
    
    // Assert - check if index buffer is available
    if (geometry.HasIndexBuffer()) {
        auto indexBuffer = geometry.GetIndexBuffer();
        EXPECT_GT(indexBuffer.size(), 0) << "Index buffer should have data";
        
        std::cout << "Index buffer loaded with " << indexBuffer.size() << " indices" << std::endl;
        
        // Verify that indices are reasonable (within vertex count range)
        // For a cube, we typically expect 36 indices (6 faces * 2 triangles * 3 vertices)
        auto vertexData = geometry.GetRawVertexData();
        size_t vertexCount = vertexData.size() / 24; // 24 bytes per vertex (position + normal)
        
        for (uint32_t index : indexBuffer) {
            EXPECT_LT(index, vertexCount) << "Index " << index << " is out of range for " << vertexCount << " vertices";
        }
    } else {
        std::cout << "No index buffer found in box.glb (geometry uses direct vertex rendering)" << std::endl;
        // This is not necessarily an error - some models don't use indices
    }
}

TEST_F(GeometryTest, LoadGLTF_TangentAndBitangent_HandledGracefully) {
    // Arrange
    auto testAssetsPath = GetTestAssetsPath();
    auto boxPath = testAssetsPath / "box.glb";
    
    ASSERT_TRUE(std::filesystem::exists(boxPath));
    
    // Define attributes including tangent and bitangent (even if not available)
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 36},   // glm::vec3 at offset 0
        {AttributeType::Normal, 0, 12, 12, 36},    // glm::vec3 at offset 12
        {AttributeType::Tangent, 0, 12, 24, 36},   // glm::vec3 at offset 24
        {AttributeType::Bitangent, 1, 12, 0, 12}   // glm::vec3 in separate buffer
    };
    
    // Act
    auto result = Geometry::LoadGLTF(boxPath, requestedAttributes);
    
    // Assert
    ASSERT_TRUE(result.has_value()) << "LoadGLTF should succeed even if some attributes are missing";
    
    const auto& geometry = result.value();
    auto attributes = geometry.GetAttributes();
    
    // Count which attributes were actually loaded
    bool hasPosition = false, hasNormal = false, hasTangent = false, hasBitangent = false;
    
    std::cout << "Requested tangent/bitangent attributes. Found " << attributes.size() << " attributes:" << std::endl;
    
    for (const auto& attr : attributes) {
        switch (attr.m_type) {
            case AttributeType::Position:
                hasPosition = true;
                std::cout << "  -> Position attribute loaded" << std::endl;
                break;
            case AttributeType::Normal:
                hasNormal = true;
                std::cout << "  -> Normal attribute loaded" << std::endl;
                break;
            case AttributeType::Tangent:
                hasTangent = true;
                std::cout << "  -> Tangent attribute loaded" << std::endl;
                break;
            case AttributeType::Bitangent:
                hasBitangent = true;
                std::cout << "  -> Bitangent attribute loaded" << std::endl;
                break;
            default:
                std::cout << "  -> Other attribute type: " << static_cast<int>(attr.m_type) << std::endl;
                break;
        }
    }
    
    // We expect at least position and normal to be loaded from box.glb
    EXPECT_TRUE(hasPosition) << "Position should be available in box.glb";
    EXPECT_TRUE(hasNormal) << "Normal should be available in box.glb";
    
    // Tangent and bitangent may or may not be available - just verify no crash occurred
    if (!hasTangent) {
        std::cout << "  -> Tangent attribute not found in box.glb (expected)" << std::endl;
    }
    if (!hasBitangent) {
        std::cout << "  -> Bitangent attribute not found in box.glb (expected)" << std::endl;
    }
}

TEST_F(GeometryTest, FromBuffers_ValidData_Success) {
    // Arrange - Create test vertex data
    std::vector<float> positions = {
        // Triangle vertices (x, y, z)
        0.0f, 1.0f, 0.0f,   // Top
        -1.0f, -1.0f, 0.0f, // Bottom left
        1.0f, -1.0f, 0.0f   // Bottom right
    };
    
    std::vector<float> normals = {
        // All normals pointing forward (x, y, z)
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };
    
    std::vector<float> texCoords = {
        // Texture coordinates (u, v)
        0.5f, 1.0f,  // Top
        0.0f, 0.0f,  // Bottom left
        1.0f, 0.0f   // Bottom right
    };
    
    std::vector<index_t> indices = {
        0, 1, 2  // Single triangle
    };
    
    // Create GeometryBuffers
    GeometryBuffers buffers;
    buffers.positions = std::span<float const>(positions);
    buffers.normals = std::span<float const>(normals);
    buffers.texCoords = std::span<float const>(texCoords);
    buffers.indices = std::span<index_t const>(indices);
    
    // Define attributes we want (interleaved layout)
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 32},   // glm::vec3 at offset 0
        {AttributeType::Normal, 0, 12, 12, 32},    // glm::vec3 at offset 12
        {AttributeType::TexCoord, 0, 8, 24, 32}    // glm::vec2 at offset 24
    };
    
    // Act
    auto result = Geometry::FromBuffers(buffers, requestedAttributes);
    
    // Assert
    ASSERT_TRUE(result.has_value()) << "FromBuffers should succeed with valid data";
    
    const auto& geometry = result.value();
    
    // Verify attributes
    auto attributes = geometry.GetAttributes();
    EXPECT_EQ(attributes.size(), 3) << "Should have 3 attributes";
    
    // Verify vertex data
    auto vertexData = geometry.GetRawVertexData();
    EXPECT_GT(vertexData.size(), 0) << "Should have vertex data";
    EXPECT_EQ(vertexData.size(), 3 * 32) << "Should have 3 vertices * 32 bytes per vertex";
    
    // Verify index buffer
    EXPECT_TRUE(geometry.HasIndexBuffer()) << "Should have index buffer";
    auto indexBuffer = geometry.GetIndexBuffer();
    EXPECT_EQ(indexBuffer.size(), 3) << "Should have 3 indices";
    
    // Verify we can access attributes through views
    try {
        auto posView = geometry.GetConstView<glm::vec3>(AttributeType::Position);
        auto normalView = geometry.GetConstView<glm::vec3>(AttributeType::Normal);
        auto texCoordView = geometry.GetConstView<glm::vec2>(AttributeType::TexCoord);
        
        // Check first vertex data
        EXPECT_NEAR(posView[0].x, 0.0f, 1e-6f);
        EXPECT_NEAR(posView[0].y, 1.0f, 1e-6f);
        EXPECT_NEAR(posView[0].z, 0.0f, 1e-6f);
        
        EXPECT_NEAR(normalView[0].x, 0.0f, 1e-6f);
        EXPECT_NEAR(normalView[0].y, 0.0f, 1e-6f);
        EXPECT_NEAR(normalView[0].z, 1.0f, 1e-6f);
        
        EXPECT_NEAR(texCoordView[0].x, 0.5f, 1e-6f);
        EXPECT_NEAR(texCoordView[0].y, 1.0f, 1e-6f);
        
        std::cout << "FromBuffers successfully created geometry with " 
                  << attributes.size() << " attributes and " 
                  << indexBuffer.size() << " indices" << std::endl;
                  
    } catch (const std::exception& e) {
        FAIL() << "Failed to access geometry views: " << e.what();
    }
}

TEST_F(GeometryTest, FromBuffers_EmptyBuffers_Failure) {
    // Arrange - Empty buffers
    GeometryBuffers buffers; // All optional fields are nullopt
    
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 12}
    };
    
    // Act
    auto result = Geometry::FromBuffers(buffers, requestedAttributes);
    
    // Assert
    EXPECT_FALSE(result.has_value()) << "Should fail with empty buffers";
    if (!result.has_value()) {
        EXPECT_TRUE(result.error().IsError()) << "Should return an error";
        EXPECT_NE(result.error().Str().find("No vertex data provided"), std::string::npos) 
            << "Error should mention no vertex data";
    }
}

TEST_F(GeometryTest, FromBuffers_MismatchedVertexCounts_Failure) {
    // Arrange - Mismatched buffer sizes
    std::vector<float> positions = {
        0.0f, 1.0f, 0.0f,   // 1 vertex
        -1.0f, -1.0f, 0.0f
    };
    
    std::vector<float> normals = {
        0.0f, 0.0f, 1.0f,   // 1 vertex (only 3 floats, not 6)
    };
    
    GeometryBuffers buffers;
    buffers.positions = std::span<float const>(positions);
    buffers.normals = std::span<float const>(normals);
    
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 24},
        {AttributeType::Normal, 0, 12, 12, 24}
    };
    
    // Act
    auto result = Geometry::FromBuffers(buffers, requestedAttributes);
    
    // Assert
    EXPECT_FALSE(result.has_value()) << "Should fail with mismatched vertex counts";
    if (!result.has_value()) {
        EXPECT_TRUE(result.error().IsError()) << "Should return an error";
        EXPECT_NE(result.error().Str().find("vertex count mismatch"), std::string::npos) 
            << "Error should mention vertex count mismatch";
    }
}

TEST_F(GeometryTest, FromBuffers_TangentWith3Components_Success) {
    // Arrange - Create test vertex data with tangents
    std::vector<float> positions = {
        0.0f, 1.0f, 0.0f,   // Top vertex
        -1.0f, -1.0f, 0.0f, // Bottom left
        1.0f, -1.0f, 0.0f   // Bottom right
    };
    
    std::vector<float> tangents = {
        // Tangent vectors (x, y, z) - 3 components each
        1.0f, 0.0f, 0.0f,   // Right direction for top vertex
        1.0f, 0.0f, 0.0f,   // Right direction for bottom left
        1.0f, 0.0f, 0.0f    // Right direction for bottom right
    };
    
    // Create GeometryBuffers
    GeometryBuffers buffers;
    buffers.positions = std::span<float const>(positions);
    buffers.tangents = std::span<float const>(tangents);
    
    // Define attributes (position + tangent)
    std::vector<Attribute> requestedAttributes = {
        {AttributeType::Position, 0, 12, 0, 24},   // glm::vec3 at offset 0
        {AttributeType::Tangent, 0, 12, 12, 24}    // glm::vec3 at offset 12 (3 components)
    };
    
    // Act
    auto result = Geometry::FromBuffers(buffers, requestedAttributes);
    
    // Assert
    ASSERT_TRUE(result.has_value()) << "FromBuffers should succeed with 3-component tangents";
    
    const auto& geometry = result.value();
    auto attributes = geometry.GetAttributes();
    EXPECT_EQ(attributes.size(), 2) << "Should have 2 attributes (position + tangent)";
    
    // Verify we can access tangent data
    try {
        auto tangentView = geometry.GetConstView<glm::vec3>(AttributeType::Tangent);
        
        // Check first vertex tangent
        EXPECT_NEAR(tangentView[0].x, 1.0f, 1e-6f);
        EXPECT_NEAR(tangentView[0].y, 0.0f, 1e-6f);
        EXPECT_NEAR(tangentView[0].z, 0.0f, 1e-6f);
        
        std::cout << "Successfully created geometry with 3-component tangents" << std::endl;
        
    } catch (const std::exception& e) {
        FAIL() << "Failed to access tangent view: " << e.what();
    }
}

TEST_F(GeometryTest, AsFormat_InterleaveToSeparate_Success) {
    // Arrange - create interleaved geometry
    std::vector<float> interleavedData = {
        // Vertex 0: pos(x,y,z), normal(x,y,z)
        0.0f, 1.0f, 0.0f,   0.0f, 0.0f, 1.0f,
        // Vertex 1: pos(x,y,z), normal(x,y,z)  
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        // Vertex 2: pos(x,y,z), normal(x,y,z)
        1.0f, -1.0f, 0.0f,   0.0f, 0.0f, 1.0f
    };

    GeometryBuffers buffers;
    buffers.positions = std::span<float const>(interleavedData.data(), 9); // 3 vertices * 3 floats
    buffers.normals = std::span<float const>(interleavedData.data() + 3, 9); // offset by 3, then 3 vertices * 3 floats

    // Create interleaved layout
    std::vector<Attribute> interleavedAttributes = {
        {AttributeType::Position, 0, 12, 0, 24},   // Buffer 0, 12 bytes, offset 0, stride 24
        {AttributeType::Normal, 0, 12, 12, 24}     // Buffer 0, 12 bytes, offset 12, stride 24
    };

    auto interleavedResult = Geometry::FromBuffers(buffers, interleavedAttributes);
    ASSERT_TRUE(interleavedResult.has_value());
    auto interleavedGeometry = std::move(interleavedResult.value());

    // Define separate buffer layout
    std::vector<Attribute> separateAttributes = {
        {AttributeType::Position, 0, 12, 0, 12},   // Buffer 0, 12 bytes, offset 0, stride 12
        {AttributeType::Normal, 1, 12, 0, 12}      // Buffer 1, 12 bytes, offset 0, stride 12
    };

    // Act - convert to separate buffers
    auto separateGeometry = interleavedGeometry.AsFormat(separateAttributes);

    // Assert
    auto attributes = separateGeometry.GetAttributes();
    EXPECT_EQ(attributes.size(), 2);

    // Check that we have two separate buffers
    auto posView = separateGeometry.GetConstView<glm::vec3>(AttributeType::Position);
    auto normalView = separateGeometry.GetConstView<glm::vec3>(AttributeType::Normal);

    // Verify data integrity
    EXPECT_FLOAT_EQ(posView[0].x, 0.0f);
    EXPECT_FLOAT_EQ(posView[0].y, 1.0f);
    EXPECT_FLOAT_EQ(posView[0].z, 0.0f);

    EXPECT_FLOAT_EQ(normalView[0].x, 0.0f);
    EXPECT_FLOAT_EQ(normalView[0].y, 0.0f);
    EXPECT_FLOAT_EQ(normalView[0].z, 1.0f);
}

TEST_F(GeometryTest, AsFormat_MissingAttribute_SkipsGracefully) {
    // Arrange - geometry with only position
    std::vector<float> positions = {0.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f};
    
    GeometryBuffers buffers;
    buffers.positions = std::span<float const>(positions);

    std::vector<Attribute> sourceAttributes = {
        {AttributeType::Position, 0, 12, 0, 12}
    };

    auto sourceResult = Geometry::FromBuffers(buffers, sourceAttributes);
    ASSERT_TRUE(sourceResult.has_value());
    auto sourceGeometry = std::move(sourceResult.value());

    // Define target layout that includes normal (which doesn't exist in source)
    std::vector<Attribute> targetAttributes = {
        {AttributeType::Position, 0, 12, 0, 24},
        {AttributeType::Normal, 0, 12, 12, 24}   // This doesn't exist in source
    };

    // Act
    auto targetGeometry = sourceGeometry.AsFormat(targetAttributes);

    // Assert - should only have position attribute
    auto attributes = targetGeometry.GetAttributes();
    EXPECT_EQ(attributes.size(), 1);
    EXPECT_EQ(attributes[0].m_type, AttributeType::Position);
}
