#include <gtest/gtest.h>
#include "../physics.hpp"
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace okami;

TEST(TransformTest, DefaultConstructor) {
    Transform t;
    EXPECT_EQ(t.m_position, glm::vec3(0.0f));
    EXPECT_EQ(t.m_rotation, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    EXPECT_EQ(t.m_scaleShear, glm::mat3(1.0f));
}

TEST(TransformTest, TransformPoint) {
    Transform t(glm::vec3(1.0f, 2.0f, 3.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::mat3(1.0f));
    glm::vec3 point(1.0f, 1.0f, 1.0f);
    glm::vec3 transformedPoint = t.TransformPoint(point);
    EXPECT_EQ(transformedPoint, glm::vec3(2.0f, 3.0f, 4.0f));
}

TEST(TransformTest, TransformVector) {
    Transform t(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::mat3(2.0f));
    glm::vec3 vector(1.0f, 1.0f, 1.0f);
    glm::vec3 transformedVector = t.TransformVector(vector);
    EXPECT_EQ(transformedVector, glm::vec3(2.0f, 2.0f, 2.0f));
}

TEST(TransformTest, AsMatrix) {
    Transform t(glm::vec3(1.0f, 2.0f, 3.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::mat3(1.0f));
    glm::mat4 matrix = t.AsMatrix();
    EXPECT_EQ(matrix[3][0], 1.0f);
    EXPECT_EQ(matrix[3][1], 2.0f);
    EXPECT_EQ(matrix[3][2], 3.0f);
    EXPECT_EQ(matrix[3][3], 1.0f);
}

TEST(TransformTest, Inverse) {
    Transform t(glm::vec3(1.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), 2.0f);
    Transform inverse = t.Inverse();
    glm::vec3 point(3.0f, 0.0f, 0.0f);
    glm::vec3 transformedPoint = t.TransformPoint(point);
    glm::vec3 originalPoint = inverse.TransformPoint(transformedPoint);
    EXPECT_EQ(originalPoint, point);
}

TEST(TransformTest, Multiplication) {
    Transform t1(glm::vec3(1.0f, 0.0f, 0.0f));
    Transform t2(glm::vec3(0.0f, 1.0f, 0.0f));
    Transform result = t1 * t2;
    EXPECT_EQ(result.m_position, glm::vec3(1.0f, 1.0f, 0.0f));
}

class PhysicsModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        en.Startup();
    }

    void TearDown() override {
        en.Shutdown();
    }

    Engine en;
};

TEST_F(PhysicsModuleTest, TransformStorage) {
	auto& entityTree = en.GetEntityTree();
    auto& signalBus = en.GetSignalBus();

	entityTree.BeginUpdates(signalBus);
	entity_t entity1 = entityTree.CreateEntity();
    entity_t entity2 = entityTree.CreateEntity();
    entityTree.EndUpdates();

	signalBus.AddComponent(entity1, Transform(glm::vec3(1.0f, 2.0f, 3.0f)));
	signalBus.AddComponent(entity2, Transform(glm::vec3(4.0f, 5.0f, 6.0f)));

    en.Run(1);

    auto storage = en.GetStorageAccessor<Transform>();
	EXPECT_EQ(storage->Get(entity1).m_position, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(storage->Get(entity2).m_position, glm::vec3(4.0f, 5.0f, 6.0f));

	signalBus.UpdateComponent(entity1, Transform(glm::vec3(7.0f, 8.0f, 9.0f)));

    en.Run(1);

	EXPECT_EQ(storage->Get(entity1).m_position, glm::vec3(7.0f, 8.0f, 9.0f));

	signalBus.RemoveComponent<Transform>(entity2);

    en.Run(1);

	EXPECT_THROW(storage->Get(entity2), std::runtime_error);

	entityTree.BeginUpdates(signalBus);
    entityTree.RemoveEntity(entity1);
    entityTree.EndUpdates();

    en.Run(1);

	EXPECT_THROW(storage->Get(entity1), std::runtime_error);
}