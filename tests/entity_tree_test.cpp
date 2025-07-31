#include <gtest/gtest.h>
#include "../entity_tree.hpp"
#include "../engine.hpp"

#include <queue>

using namespace okami;

class EntityTreeTest : public ::testing::Test {
protected:
	void SetUp() override {
		signalHandlers = std::make_unique<SignalHandlerCollection>();
		world = std::make_unique<EntityTree>();
	}

	void TearDown() override {
		world.reset();
	}

	std::unique_ptr<EntityTree> world;
	std::unique_ptr<SignalHandlerCollection> signalHandlers;
};

// Basic Entity Creation Tests
TEST_F(EntityTreeTest, CreateEntityTest) {
	entity_t entity1 = world->CreateEntity(*signalHandlers);
	entity_t entity2 = world->CreateEntity(*signalHandlers);

	EXPECT_NE(entity1, entity2);
	EXPECT_NE(entity1, kNullEntity);
	EXPECT_NE(entity2, kNullEntity);
}

TEST_F(EntityTreeTest, CreateEntityWithParentTest) {
	entity_t parent = world->CreateEntity(*signalHandlers);
	entity_t child = world->CreateEntity(*signalHandlers, parent);

	EXPECT_EQ(world->GetParent(child), parent);
	EXPECT_EQ(world->GetParent(parent), kRoot);
}

// Hierarchy Tests
TEST_F(EntityTreeTest, SetParentTest) {
	entity_t entity1 = world->CreateEntity(*signalHandlers);
	entity_t entity2 = world->CreateEntity(*signalHandlers);
	entity_t entity3 = world->CreateEntity(*signalHandlers);

	// Set entity3 as child of entity1
	world->SetParent(*signalHandlers, entity3, entity1);
	EXPECT_EQ(world->GetParent(entity3), entity1);

	// Move entity3 to be child of entity2
	world->SetParent(*signalHandlers, entity3, entity2);
	EXPECT_EQ(world->GetParent(entity3), entity2);
}

TEST_F(EntityTreeTest, CircularDependencyPreventionTest) {
	entity_t entity1 = world->CreateEntity(*signalHandlers);
	entity_t entity2 = world->CreateEntity(*signalHandlers, entity1);
	entity_t entity3 = world->CreateEntity(*signalHandlers, entity2);

	// Try to create circular dependency: entity1 -> entity2 -> entity3 -> entity1
	world->SetParent(*signalHandlers, entity1, entity3);

	// Should not change - circular dependency prevented
	EXPECT_EQ(world->GetParent(entity1), kRoot);
	EXPECT_EQ(world->GetParent(entity2), entity1);
	EXPECT_EQ(world->GetParent(entity3), entity2);
}

TEST_F(EntityTreeTest, CannotReparentRootTest) {
	entity_t entity1 = world->CreateEntity(*signalHandlers);
	EXPECT_ANY_THROW(world->SetParent(*signalHandlers, kRoot, entity1));

	// Root should still have no parent
	EXPECT_EQ(world->GetParent(kRoot), kNullEntity);
}

// Entity Removal Tests
TEST_F(EntityTreeTest, RemoveEntityTest) {
	entity_t entity1 = world->CreateEntity(*signalHandlers);
	entity_t child1 = world->CreateEntity(*signalHandlers, entity1);
	entity_t child2 = world->CreateEntity(*signalHandlers, entity1);

	world->RemoveEntity(*signalHandlers, entity1);
}

TEST_F(EntityTreeTest, CannotRemoveRootTest) {
	EXPECT_ANY_THROW(world->RemoveEntity(*signalHandlers, kRoot));

	// Root should still exist
	EXPECT_EQ(world->GetParent(kRoot), kNullEntity);
}

// Iterator Tests
TEST_F(EntityTreeTest, ChildrenIteratorTest) {
	entity_t parent = world->CreateEntity(*signalHandlers);
	entity_t child1 = world->CreateEntity(*signalHandlers, parent);
	entity_t child2 = world->CreateEntity(*signalHandlers, parent);
	entity_t child3 = world->CreateEntity(*signalHandlers, parent);

	std::vector<entity_t> children;
	for (auto child : world->GetChildren(parent)) {
		children.push_back(child);
	}

	EXPECT_EQ(children.size(), 3);
	EXPECT_TRUE(std::find(children.begin(), children.end(), child1) != children.end());
	EXPECT_TRUE(std::find(children.begin(), children.end(), child2) != children.end());
	EXPECT_TRUE(std::find(children.begin(), children.end(), child3) != children.end());
}

TEST_F(EntityTreeTest, ChildrenIteratorEmptyTest) {
	entity_t entity = world->CreateEntity(*signalHandlers);

	std::vector<entity_t> children;
	for (auto child : world->GetChildren(entity)) {
		children.push_back(child);
	}

	EXPECT_EQ(children.size(), 0);
}

TEST_F(EntityTreeTest, AncestorIteratorTest) {
	entity_t grandparent = world->CreateEntity(*signalHandlers);
	entity_t parent = world->CreateEntity(*signalHandlers, grandparent);
	entity_t child = world->CreateEntity(*signalHandlers, parent);

	std::vector<entity_t> ancestors;
	for (auto ancestor : world->GetAncestors(child)) {
		ancestors.push_back(ancestor);
	}

	EXPECT_EQ(ancestors.size(), 3); // parent, grandparent, root
	EXPECT_EQ(ancestors[0], parent);
	EXPECT_EQ(ancestors[1], grandparent);
	EXPECT_EQ(ancestors[2], kRoot);
}

TEST_F(EntityTreeTest, AncestorIteratorRootTest) {
	std::vector<entity_t> ancestors;
	for (auto ancestor : world->GetAncestors(kRoot)) {
		ancestors.push_back(ancestor);
	}

	EXPECT_EQ(ancestors.size(), 0);
}

TEST_F(EntityTreeTest, DescendantsIteratorTest) {
	entity_t parent = world->CreateEntity(*signalHandlers);
	entity_t child1 = world->CreateEntity(*signalHandlers, parent);
	entity_t child2 = world->CreateEntity(*signalHandlers, parent);
	entity_t grandchild1 = world->CreateEntity(*signalHandlers, child1);
	entity_t grandchild2 = world->CreateEntity(*signalHandlers, child2);

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

TEST_F(EntityTreeTest, DescendantsIteratorEmptyTest) {
	entity_t entity = world->CreateEntity(*signalHandlers);

	std::vector<entity_t> descendants;
	for (auto descendant : world->GetDescendants(entity)) {
		descendants.push_back(descendant);
	}

	EXPECT_EQ(descendants.size(), 0);
}

// Complex Hierarchy Tests
TEST_F(EntityTreeTest, ComplexHierarchyTest) {
	// Create a complex hierarchy:
	//     root
	//    /    \
    //  ent1   ent2
	//  / \      |
	// c1  c2   c3
	//     |
	//    gc1

	entity_t ent1 = world->CreateEntity(*signalHandlers);
	entity_t ent2 = world->CreateEntity(*signalHandlers);
	entity_t c1 = world->CreateEntity(*signalHandlers, ent1);
	entity_t c2 = world->CreateEntity(*signalHandlers, ent1);
	entity_t c3 = world->CreateEntity(*signalHandlers, ent2);
	entity_t gc1 = world->CreateEntity(*signalHandlers, c2);

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
TEST_F(EntityTreeTest, InvalidEntityOperationsTest) {
#ifndef NDEBUG
    entity_t invalidEntity = 9999;

    // Operations on invalid entities should be safe
    EXPECT_ANY_THROW(world->GetParent(invalidEntity));
    EXPECT_ANY_THROW(world->SetParent(*signalHandlers, invalidEntity, kRoot));
    EXPECT_ANY_THROW(world->RemoveEntity(*signalHandlers, invalidEntity));

    // Iterators should be empty for invalid entities
    std::vector<entity_t> children;
    EXPECT_ANY_THROW({
        for (auto child : world->GetChildren(invalidEntity)) {
            children.push_back(child);
        }
    });
    EXPECT_EQ(children.size(), 0);
#endif
}

TEST_F(EntityTreeTest, SetParentToInvalidEntityTest) {
#ifndef NDEBUG
    entity_t entity = world->CreateEntity(*signalHandlers);
    entity_t invalidParent = 9999;

    EXPECT_ANY_THROW(world->SetParent(*signalHandlers, entity, invalidParent));

    // Should not change parent
    EXPECT_EQ(world->GetParent(entity), kRoot);
#endif
}

// Iterator Increment Tests
TEST_F(EntityTreeTest, IteratorIncrementTest) {
    entity_t parent = world->CreateEntity(*signalHandlers);
    entity_t child1 = world->CreateEntity(*signalHandlers, parent);
    entity_t child2 = world->CreateEntity(*signalHandlers, parent);

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

TEST_F(EntityTreeTest, AncestorIteratorIncrementTest) {
    entity_t grandparent = world->CreateEntity(*signalHandlers);
    entity_t parent = world->CreateEntity(*signalHandlers, grandparent);
    entity_t child = world->CreateEntity(*signalHandlers, parent);

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

TEST_F(EntityTreeTest, SignalTest) {
    std::queue<EntityCreateSignal> addSignals;
    std::queue<EntityParentChangeSignal> parentChangeSignals;
    std::queue<EntityRemoveSignal> removeSignals;

    signalHandlers->RegisterHandler<EntityCreateSignal>([&](EntityCreateSignal signal) {
        addSignals.push(signal);
        });

    signalHandlers->RegisterHandler<EntityParentChangeSignal>([&](EntityParentChangeSignal signal) {
        parentChangeSignals.push(signal);
        });

    signalHandlers->RegisterHandler<EntityRemoveSignal>([&](EntityRemoveSignal signal) {
        removeSignals.push(signal);
        });

    auto parent1 = world->CreateEntity(*signalHandlers);
    auto parent2 = world->CreateEntity(*signalHandlers);
    auto child1 = world->CreateEntity(*signalHandlers, parent1);

    world->SetParent(*signalHandlers, child1, parent2); // Change parent to trigger parent change signal

    world->RemoveEntity(*signalHandlers, child1);
    world->RemoveEntity(*signalHandlers, parent1);
    world->RemoveEntity(*signalHandlers, parent2);

    // Check add signals
    EXPECT_EQ(addSignals.size(), 3);
    EXPECT_EQ(addSignals.front().m_entity, parent1);
    addSignals.pop();
    EXPECT_EQ(addSignals.front().m_entity, parent2);
    addSignals.pop();
    EXPECT_EQ(addSignals.front().m_entity, child1);
    addSignals.pop();
    // Check parent change signals
    {
        EXPECT_EQ(parentChangeSignals.size(), 1); // No parent changes in this test
        auto front = parentChangeSignals.front();
        EXPECT_EQ(front.m_entity, child1);
        EXPECT_EQ(front.m_oldParent, parent1);
        EXPECT_EQ(front.m_newParent, parent2);
    }
    // Check remove signals
    EXPECT_EQ(removeSignals.size(), 3);
    EXPECT_EQ(removeSignals.front().m_entity, child1);
    removeSignals.pop();
    EXPECT_EQ(removeSignals.front().m_entity, parent1);
    removeSignals.pop();
}