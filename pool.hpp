#pragma once

#include <type_traits>
#include <vector>
#include <set>

#include "assert.hpp"

namespace okami {
	template <typename T, typename IndexT = int>
	class Pool final {
	private:
		std::vector<T> m_objects;
		std::set<IndexT> m_freeIndices;

	public:
		IndexT Allocate() {
			if (m_freeIndices.empty()) {
				// Allocate a new object
				m_objects.emplace_back();
				IndexT index = static_cast<IndexT>(m_objects.size() - 1);
				return index;
			}
			else {
				// Reuse a free index
				auto it = m_freeIndices.begin();
				IndexT index = *it;
				m_freeIndices.erase(it);
				return index;
			}
		}

		bool IsFree(IndexT index) const {
			return index < 0 || index >= static_cast<IndexT>(m_objects.size()) || m_freeIndices.find(index) != m_freeIndices.end();
		}

		void Free(IndexT index) {
			OKAMI_ASSERT(!IsFree(index), "Cannot free an already freed object or an invalid index");

			// Add to free indices
			m_freeIndices.insert(index);

			// If the index is at the end of the vector, we can pop it
			// Keep doing this while the last element is free
			while (!m_freeIndices.empty() && !m_objects.empty()) {
				IndexT lastIndex = static_cast<IndexT>(m_objects.size() - 1);
				auto it = m_freeIndices.rbegin();
				if (*it == lastIndex) {
					m_freeIndices.erase(*it);
					m_objects.pop_back();
				}
				else {
					break; // No more contiguous free indices at the end
				}
			}
		}

		T& operator[](IndexT index) {
			OKAMI_ASSERT(index >= 0 && index < static_cast<IndexT>(m_objects.size()), "Index out of bounds");
			OKAMI_ASSERT(!IsFree(index), "Accessing freed object in pool");
			return m_objects[index];
		}

		T const& operator[](IndexT index) const {
			OKAMI_ASSERT(index >= 0 && index < static_cast<IndexT>(m_objects.size()), "Index out of bounds");
			OKAMI_ASSERT(!IsFree(index), "Accessing freed object in pool");
			return m_objects[index];
		}

		// Additional utility methods for testing and debugging
		size_t Size() const {
			return m_objects.size();
		}

		size_t FreeCount() const {
			return m_freeIndices.size();
		}

		size_t ActiveCount() const {
			return m_objects.size() - m_freeIndices.size();
		}

		void Clear() {
			m_objects.clear();
			m_freeIndices.clear();
		}
	};
}