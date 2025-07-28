#include <gtest/gtest.h>
#include "../world.hpp"

using namespace okami;

class WorldTest : public ::testing::Test {
protected:
    void SetUp() override {
		signalHandlers = std::make_unique<SignalHandlerCollection>();
        world = std::make_unique<World>(signalHandlers.get());
    }

    void TearDown() override {
        world.reset();
    }

    std::unique_ptr<World> world;
	std::unique_ptr<SignalHandlerCollection> signalHandlers;
};

// Basic Entity Creation Tests
TEST_F(WorldTest, CreateEntityTest) {
    entity_t entity1 = world->CreateEntity();
    entity_t entity2 = world->CreateEntity();
    
    EXPECT_NE(entity1, entity2);
    EXPECT_NE(entity1, kNullEntity);
    EXPECT_NE(entity2, kNullEntity);
}

TEST_F(WorldTest, CreateEntityWithParentTest) {
    entity_t parent = world->CreateEntity();
    entity_t child = world->CreateEntity(parent);
    
    EXPECT_EQ(world->GetParent(child), parent);
    EXPECT_EQ(world->GetParent(parent), kRoot);
}

// Hierarchy Tests
TEST_F(WorldTest, SetParentTest) {
    entity_t entity1 = world->CreateEntity();
    entity_t entity2 = world->CreateEntity();
    entity_t entity3 = world->CreateEntity();
    
    // Set entity3 as child of entity1
    world->SetParent(entity3, entity1);
    EXPECT_EQ(world->GetParent(entity3), entity1);
    
    // Move entity3 to be child of entity2
    world->SetParent(entity3, entity2);
    EXPECT_EQ(world->GetParent(entity3), entity2);
}

TEST_F(WorldTest, CircularDependencyPreventionTest) {
    entity_t entity1 = world->CreateEntity();
    entity_t entity2 = world->CreateEntity(entity1);
    entity_t entity3 = world->CreateEntity(entity2);
    
    // Try to create circular dependency: entity1 -> entity2 -> entity3 -> entity1
    world->SetParent(entity1, entity3);
    
    // Should not change - circular dependency prevented
    EXPECT_EQ(world->GetParent(entity1), kRoot);
    EXPECT_EQ(world->GetParent(entity2), entity1);
    EXPECT_EQ(world->GetParent(entity3), entity2);
}

TEST_F(WorldTest, CannotReparentRootTest) {
    entity_t entity1 = world->CreateEntity();
    world->SetParent(kRoot, entity1);
    
    // Root should still have no parent
    EXPECT_EQ(world->GetParent(kRoot), kNullEntity);
}

// Entity Removal Tests
TEST_F(WorldTest, RemoveEntityTest) {
    entity_t entity1 = world->CreateEntity();
    entity_t child1 = world->CreateEntity(entity1);
    entity_t child2 = world->CreateEntity(entity1);
    
    world->RemoveEntity(entity1);
    
    // All should be removed
    EXPECT_EQ(world->GetParent(entity1), kNullEntity);
    EXPECT_EQ(world->GetParent(child1), kNullEntity);
    EXPECT_EQ(world->GetParent(child2), kNullEntity);
}

TEST_F(WorldTest, CannotRemoveRootTest) {
    world->RemoveEntity(kRoot);
    
    // Root should still exist
    EXPECT_EQ(world->GetParent(kRoot), kNullEntity);
}

TEST_F(WorldTest, RemoveEntityRecursivelyTest) {
    entity_t parent = world->CreateEntity();
    entity_t child = world->CreateEntity(parent);
    entity_t grandchild = world->CreateEntity(child);
    
    world->RemoveEntity(parent);
    
    // All descendants should be removed
    EXPECT_EQ(world->GetParent(parent), kNullEntity);
    EXPECT_EQ(world->GetParent(child), kNullEntity);
    EXPECT_EQ(world->GetParent(grandchild), kNullEntity);
}

// Iterator Tests
TEST_F(WorldTest, ChildrenIteratorTest) {
    entity_t parent = world->CreateEntity();
    entity_t child1 = world->CreateEntity(parent);
    entity_t child2 = world->CreateEntity(parent);
    entity_t child3 = world->CreateEntity(parent);
    
    std::vector<entity_t> children;
    for (auto child : world->GetChildren(parent)) {
        children.push_back(child);
    }
    
    EXPECT_EQ(children.size(), 3);
    EXPECT_TRUE(std::find(children.begin(), children.end(), child1) != children.end());
    EXPECT_TRUE(std::find(children.begin(), children.end(), child2) != children.end());
    EXPECT_TRUE(std::find(children.begin(), children.end(), child3) != children.end());
}

TEST_F(WorldTest, ChildrenIteratorEmptyTest) {
    entity_t entity = world->CreateEntity();
    
    std::vector<entity_t> children;
    for (auto child : world->GetChildren(entity)) {
        children.push_back(child);
    }
    
    EXPECT_EQ(children.size(), 0);
}

TEST_F(WorldTest, AncestorIteratorTest) {
    entity_t grandparent = world->CreateEntity();
    entity_t parent = world->CreateEntity(grandparent);
    entity_t child = world->CreateEntity(parent);
    
    std::vector<entity_t> ancestors;
    for (auto ancestor : world->GetAncestors(child)) {
        ancestors.push_back(ancestor);
    }
    
    EXPECT_EQ(ancestors.size(), 3); // parent, grandparent, root
    EXPECT_EQ(ancestors[0], parent);
    EXPECT_EQ(ancestors[1], grandparent);
    EXPECT_EQ(ancestors[2], kRoot);
}

TEST_F(WorldTest, AncestorIteratorRootTest) {
    std::vector<entity_t> ancestors;
    for (auto ancestor : world->GetAncestors(kRoot)) {
        ancestors.push_back(ancestor);
    }
    
    EXPECT_EQ(ancestors.size(), 0);
}

TEST_F(WorldTest, DescendantsIteratorTest) {
    entity_t parent = world->CreateEntity();
    entity_t child1 = world->CreateEntity(parent);
    entity_t child2 = world->CreateEntity(parent);
    entity_t grandchild1 = world->CreateEntity(child1);
    entity_t grandchild2 = world->CreateEntity(child2);
    
    std::vector<entity_t> descendants;
    for (auto descendant : world->GetDescendants(parent)) {
        descendants.push_back(descendant);
    }
    
    EXPECT_EQ(descendants.size(), 4);
    
    // Should be in prefix order: child1, grandchild1, child2, grandchild2
    EXPECT_TRUE(std::find(descendants.begin(), descendants.end(), child1) != descendants.end());
    EXPECT_TRUE(std::find(descendants.begin(), descendants.end(), child2) != descendants.end());
    EXPECT_TRUE(std::find(descendants.begin(), descendants.end(), grandchild1) != descendants.end());
    EXPECT_TRUE(std::find(descendants.begin(), descendants.end(), grandchild2) != descendants.end());
}

TEST_F(WorldTest, DescendantsIteratorEmptyTest) {
    entity_t entity = world->CreateEntity();
    
    std::vector<entity_t> descendants;
    for (auto descendant : world->GetDescendants(entity)) {
        descendants.push_back(descendant);
    }
    
    EXPECT_EQ(descendants.size(), 0);
}

// Complex Hierarchy Tests
TEST_F(WorldTest, ComplexHierarchyTest) {
    // Create a complex hierarchy:
    //     root
    //    /    \
    //  ent1   ent2
    //  / \      |
    // c1  c2   c3
    //     |
    //    gc1
    
    entity_t ent1 = world->CreateEntity();
    entity_t ent2 = world->CreateEntity();
    entity_t c1 = world->CreateEntity(ent1);
    entity_t c2 = world->CreateEntity(ent1);
    entity_t c3 = world->CreateEntity(ent2);
    entity_t gc1 = world->CreateEntity(c2);
    
    // Test parent relationships
    EXPECT_EQ(world->GetParent(ent1), kRoot);
    EXPECT_EQ(world->GetParent(ent2), kRoot);
    EXPECT_EQ(world->GetParent(c1), ent1);
    EXPECT_EQ(world->GetParent(c2), ent1);
    EXPECT_EQ(world->GetParent(c3), ent2);
    EXPECT_EQ(world->GetParent(gc1), c2);
    
    // Test children count
    std::vector<entity_t> rootChildren;
    for (auto child : world->GetChildren(kRoot)) {
        rootChildren.push_back(child);
    }
    EXPECT_EQ(rootChildren.size(), 2);
    
    std::vector<entity_t> ent1Children;
    for (auto child : world->GetChildren(ent1)) {
        ent1Children.push_back(child);
    }
    EXPECT_EQ(ent1Children.size(), 2);
    
    std::vector<entity_t> ent2Children;
    for (auto child : world->GetChildren(ent2)) {
        ent2Children.push_back(child);
    }
    EXPECT_EQ(ent2Children.size(), 1);
}

// Edge Case Tests
TEST_F(WorldTest, InvalidEntityOperationsTest) {
    entity_t invalidEntity = 9999;
    
    // Operations on invalid entities should be safe
    EXPECT_EQ(world->GetParent(invalidEntity), kNullEntity);
    world->SetParent(invalidEntity, kRoot); // Should not crash
    world->RemoveEntity(invalidEntity); // Should not crash
    
    // Iterators should be empty for invalid entities
    std::vector<entity_t> children;
    for (auto child : world->GetChildren(invalidEntity)) {
        children.push_back(child);
    }
    EXPECT_EQ(children.size(), 0);
}

TEST_F(WorldTest, SetParentToInvalidEntityTest) {
    entity_t entity = world->CreateEntity();
    entity_t invalidParent = 9999;
    
    world->SetParent(entity, invalidParent);
    
    // Should not change parent
    EXPECT_EQ(world->GetParent(entity), kRoot);
}

// Iterator Increment Tests
TEST_F(WorldTest, IteratorIncrementTest) {
    entity_t parent = world->CreateEntity();
    entity_t child1 = world->CreateEntity(parent);
    entity_t child2 = world->CreateEntity(parent);
    
    auto childrenIter = world->GetChildren(parent).begin();
	auto childrenEnd = world->GetChildren(parent).end();
    EXPECT_TRUE(childrenIter != childrenEnd);
    entity_t firstChild = *childrenIter;
    
    ++childrenIter;
    EXPECT_TRUE(childrenIter != childrenEnd);
    entity_t secondChild = *childrenIter;
    
    ++childrenIter;
    EXPECT_FALSE(childrenIter != childrenEnd); // Should be at end
    
    EXPECT_NE(firstChild, secondChild);
    EXPECT_TRUE(firstChild == child1 || firstChild == child2);
    EXPECT_TRUE(secondChild == child1 || secondChild == child2);
}

TEST_F(WorldTest, AncestorIteratorIncrementTest) {
    entity_t grandparent = world->CreateEntity();
    entity_t parent = world->CreateEntity(grandparent);
    entity_t child = world->CreateEntity(parent);
    
    auto ancestorIter = world->GetAncestors(child).begin();
	auto ancestorEnd = world->GetAncestors(child).end();
    EXPECT_TRUE(ancestorIter != ancestorEnd);
    EXPECT_EQ(*ancestorIter, parent);
    
    ++ancestorIter;
    EXPECT_TRUE(ancestorIter != ancestorEnd);
    EXPECT_EQ(*ancestorIter, grandparent);
    
    ++ancestorIter;
    EXPECT_TRUE(ancestorIter != ancestorEnd);
    EXPECT_EQ(*ancestorIter, kRoot);
    
    ++ancestorIter;
    EXPECT_FALSE(ancestorIter != ancestorEnd); // Should be at end
}