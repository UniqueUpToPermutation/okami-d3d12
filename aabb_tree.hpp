#pragma once

#include <glm/vec3.hpp>
#include <glm/common.hpp>
#include "pool.hpp"
#include <queue>
#include <limits>
#include <stdexcept>

namespace okami {
	struct AABB {
		glm::vec3 m_min;
		glm::vec3 m_max;

		inline bool Contains(const glm::vec3& point) const {
			return (point.x >= m_min.x && point.x <= m_max.x &&
				point.y >= m_min.y && point.y <= m_max.y &&
				point.z >= m_min.z && point.z <= m_max.z);
		}

		inline bool Contains(const AABB& other) const {
			return (m_min.x <= other.m_min.x && m_max.x >= other.m_max.x &&
				m_min.y <= other.m_min.y && m_max.y >= other.m_max.y &&
				m_min.z <= other.m_min.z && m_max.z >= other.m_max.z);
		}
	};

	inline AABB Union(const AABB& a, const AABB& b) {
		return AABB{
			glm::min(a.m_min, b.m_min),
			glm::max(a.m_max, b.m_max)
		};
	}

	inline AABB Intersection(const AABB& a, const AABB& b) {
		return AABB{
			glm::max(a.m_min, b.m_min),
			glm::min(a.m_max, b.m_max)
		};
	}

	inline bool Intersects(const AABB& a, const AABB& b) {
		return (a.m_min.x <= b.m_max.x && a.m_max.x >= b.m_min.x &&
			a.m_min.y <= b.m_max.y && a.m_max.y >= b.m_min.y &&
			a.m_min.z <= b.m_max.z && a.m_max.z >= b.m_min.z);
	}

	inline float Volume(const AABB& a) {
		return (a.m_max.x - a.m_min.x) * (a.m_max.y - a.m_min.y) * (a.m_max.z - a.m_min.z);
	}

	inline float SurfaceArea(const AABB& a) {
		float dx = a.m_max.x - a.m_min.x;
		float dy = a.m_max.y - a.m_min.y;
		float dz = a.m_max.z - a.m_min.z;
		return 2.0f * (dx * dy + dy * dz + dz * dx);
	}

	constexpr int kInvalidNodeIndex = -1;

	template <typename LeafData>
	struct AABBNode {
		AABB aabb;
		LeafData data;
		int parent = kInvalidNodeIndex;
		int left = kInvalidNodeIndex;
		int right = kInvalidNodeIndex;

		inline bool IsLeaf() const {
			return left == kInvalidNodeIndex && right == kInvalidNodeIndex;
		}
	};

	struct DefaultCostFunction {
		inline float operator()(const AABB& aabb) const {
			return SurfaceArea(aabb);
		}
	};

	template <
		typename LeafData = unsigned int,
		typename CostFunction = DefaultCostFunction
	>
	class AABBTree {
	private:
		Pool<AABBNode<LeafData>> m_nodes;
		int m_root = kInvalidNodeIndex;

		AABBNode<LeafData>& Balance(int nodeIndex) {
			auto& node = m_nodes[nodeIndex];
			auto parentIndex = node.parent;
			if (parentIndex == kInvalidNodeIndex) {
				return node;
			}
			auto& parent = m_nodes[parentIndex];
			auto grandParentIndex = parent.parent;
			if (grandParentIndex == kInvalidNodeIndex) {
				return node;
			}
			auto& grandParent = m_nodes[grandParentIndex];

			auto siblingIndex = (parent.left == nodeIndex) ? parent.right : parent.left;
			auto parentSiblingIndex = (grandParent.left == parentIndex) ? grandParent.right : grandParent.left;

			auto& sibling = m_nodes[siblingIndex];
			auto& parentSibling = m_nodes[parentSiblingIndex];

			auto parentAabbNothing = Union(node.aabb, sibling.aabb);
			auto parentAabbRotate = Union(parentSibling.aabb, sibling.aabb);

			auto costDoNothing = CostFunction{}(parentAabbNothing);
			auto costRotate = CostFunction{}(parentAabbRotate);

			if (costRotate < costDoNothing) {
				// Perform rotation
				parent.left = parentSiblingIndex;
				parent.right = siblingIndex;

				grandParent.left = parentIndex;
				grandParent.right = nodeIndex;

				node.parent = grandParentIndex;
				parentSibling.parent = parentIndex;

				parent.aabb = parentAabbRotate;
			}

			return node;
		}

		bool ValidateNode(int nodeIndex) const {
			if (nodeIndex == kInvalidNodeIndex) {
				return true;
			}
			const auto& node = m_nodes[nodeIndex];

			if (node.IsLeaf()) {
				return true;
			}
			else {
				// Non-leaf nodes should have children
				if (node.left == kInvalidNodeIndex || node.right == kInvalidNodeIndex) {
					return false; // Invalid non-leaf node
				}

				// Validate AABB
				const auto& left = m_nodes[node.left];
				const auto& right = m_nodes[node.right];
				if (!node.aabb.Contains(left.aabb) || !node.aabb.Contains(right.aabb)) {
					return false; // AABB does not contain children AABBs
				}

				// Recursively validate children
				return ValidateNode(node.left) && ValidateNode(node.right);
			}
		}

		void WalkUpAndFix(int nodeIndex) {
			while (nodeIndex != kInvalidNodeIndex) {
				auto& node = m_nodes[nodeIndex];

				auto leftIndex = node.left;
				auto rightIndex = node.right;

				if (leftIndex != kInvalidNodeIndex && rightIndex != kInvalidNodeIndex) {
					auto& left = m_nodes[leftIndex];
					auto& right = m_nodes[rightIndex];
					node.aabb = Union(left.aabb, right.aabb);
				}

				nodeIndex = Balance(nodeIndex).parent;
			}
		}

	public:
		bool Validate() const {
			if (m_root == kInvalidNodeIndex) {
				return true; // Empty tree is valid
			}
			return ValidateNode(m_root);
		}

		int Insert(const AABB& aabb, LeafData data) {
			// Allocate root node if tree is empty
			if (m_root == kInvalidNodeIndex) {
				m_root = m_nodes.Allocate();
				m_nodes[m_root] = AABBNode<LeafData>{
					.aabb = aabb,
					.data = std::move(data),
				};
				return m_root;
			}

			// Create new parent and new node
			auto newNodeIndex = m_nodes.Allocate();
			auto newParentIndex = m_nodes.Allocate();

			m_nodes[newNodeIndex] = AABBNode<LeafData>{
				.aabb = aabb,
				.data = std::move(data),
			};
			auto& newNode = m_nodes[newNodeIndex];

			// Find the best sibling for this new leaf
			auto siblingIndex = FindBestSibling(newNode.aabb);
			auto& sibling = m_nodes[siblingIndex];
			auto oldParentIndex = sibling.parent;

			auto& newParent = m_nodes[newParentIndex];

			newParent.parent = oldParentIndex;

			if (oldParentIndex != kInvalidNodeIndex) {
				auto& oldParent = m_nodes[oldParentIndex];
				// Sibling was not the root
				if (oldParent.left == siblingIndex) {
					oldParent.left = newParentIndex;
				}
				else {
					oldParent.right = newParentIndex;
				}

				newParent.left = siblingIndex;
				newParent.right = newNodeIndex;
				sibling.parent = newParentIndex;
				newNode.parent = newParentIndex;
			}
			else {
				// Sibling was the root
				newParent.left = siblingIndex;
				newParent.right = newNodeIndex;
				sibling.parent = newParentIndex;
				newNode.parent = newParentIndex;
				m_root = newParentIndex;
			}

			// Walk back up the tree fixing heights and AABBs
			WalkUpAndFix(newNode.parent);
			
			return newNodeIndex;
		}

		int FindBestSibling(const AABB& aabb) {
			// First entry is inheritance cost
			typedef std::pair<double, int> NodeCostPair;

			std::priority_queue<NodeCostPair, std::vector<NodeCostPair>, std::greater<NodeCostPair>> pq;

			int bestNodeIndex = kInvalidNodeIndex;
			double bestCost = std::numeric_limits<double>::max();

			pq.push({ 0.0, m_root });

			while (!pq.empty()) {
				auto [inheritedCost, nodeIndex] = pq.top();
				pq.pop();

				const auto& node = m_nodes[nodeIndex];

				auto mergedAABB = Union(node.aabb, aabb);

				// The cost of inserting here is the cost of the new parent AABB
				// that would be created, plus all of the ancestors that must be
				// increased in size
				auto totalCostToInsertHere = CostFunction{}(mergedAABB) + inheritedCost;

				// Update best cost option
				if (totalCostToInsertHere < bestCost) {
					bestCost = totalCostToInsertHere;
					bestNodeIndex = nodeIndex;
				}

				if (!node.IsLeaf()) {
					auto deltaInheritedCost = CostFunction{}(mergedAABB) - CostFunction{}(node.aabb);
					auto newInheritedCost = inheritedCost + deltaInheritedCost;

					if (newInheritedCost < bestCost) {
						// Only push children if they can potentially lead to a better cost
						pq.push({ newInheritedCost, node.left });
						pq.push({ newInheritedCost, node.right });
					}
				}
			}

			return bestNodeIndex;
		}

		void Remove(int leafIndex) {
			auto& leaf = m_nodes[leafIndex];

			if (!leaf.IsLeaf()) {
				throw std::runtime_error("Cannot remove a non-leaf node");
			}

			if (leafIndex == m_root) {
				m_root = kInvalidNodeIndex;
				m_nodes.Free(leafIndex);
				return;
			}

			auto parentIndex = leaf.parent;
			auto& parent = m_nodes[parentIndex];
			auto grandParentIndex = parent.parent;
			auto siblingIndex = (parent.left == leafIndex) ? parent.right : parent.left;
			auto& sibling = m_nodes[siblingIndex];

			if (grandParentIndex != kInvalidNodeIndex) {
				auto& grandParent = m_nodes[grandParentIndex];
				// Destroy parent and connect sibling to grandParent
				if (grandParent.left == parentIndex) {
					grandParent.left = siblingIndex;
				}
				else {
					grandParent.right = siblingIndex;
				}
				sibling.parent = grandParentIndex;

				// Free the removed leaf and its parent
				m_nodes.Free(leafIndex);
				m_nodes.Free(parentIndex);

				// Adjust ancestor AABBs and heights
				WalkUpAndFix(grandParentIndex);
			}
			else {
				m_root = siblingIndex;
				sibling.parent = kInvalidNodeIndex;
				
				// Free the removed leaf and its parent
				m_nodes.Free(leafIndex);
				m_nodes.Free(parentIndex);
			}
		}

		void Clear() {
			m_root = kInvalidNodeIndex;
			m_nodes.Clear();
		}
	};
}