#pragma once
#include <optional>
#include <unordered_map>
#include <vector>

template <typename T>
class MinHeap {
private:
	struct Node {
		T item;
		int priority;
	};

	std::vector<Node> elements;
	std::unordered_map<T, size_t> itemToIndexMap;

	void swap(size_t i, size_t j) {
		std::swap(elements[i], elements[j]);
		itemToIndexMap[elements[i].item] = i;
		itemToIndexMap[elements[j].item] = j;
	}

	void heapifyUp(size_t index) {
		while (index > 0) {
			size_t parent = (index - 1) / 2;
			if (elements[index].priority >= elements[parent].priority)
				break;
			swap(index, parent);
			index = parent;
		}
	}

	void heapifyDown(size_t index) {
		size_t len = elements.size();
		while (true) {
			size_t left = 2 * index + 1;
			size_t right = 2 * index + 2;
			size_t smallest = index;

			if (left < len && elements[left].priority < elements[smallest].priority)
				smallest = left;
			if (right < len && elements[right].priority < elements[smallest].priority)
				smallest = right;

			if (smallest == index)
				break;
			swap(index, smallest);
			index = smallest;
		}
	}

public:
	MinHeap() = default;

	// Insert item with priority
	void insert(const T &item, int priority) {
		elements.push_back({ item, priority });
		itemToIndexMap[item] = elements.size() - 1;
		heapifyUp(elements.size() - 1);
	}

	// Extract min item (returns optional)
	std::optional<T> extractMin() {
		if (elements.empty())
			return std::nullopt;

		Node minNode = elements[0];
		itemToIndexMap.erase(minNode.item);

		Node last = elements.back();
		elements.pop_back();

		if (!elements.empty()) {
			elements[0] = last;
			itemToIndexMap[last.item] = 0;
			heapifyDown(0);
		}

		return minNode.item;
	}

	// Peek min item
	std::optional<T> peek() const {
		if (elements.empty())
			return std::nullopt;
		return elements[0].item;
	}

	// Get size
	size_t size() const {
		return elements.size();
	}

	// Update priority
	bool updatePriority(const T &item, int newPriority) {
		auto kv = itemToIndexMap.find(item);
		if (kv == itemToIndexMap.end())
			return false; // not found

		size_t index = kv->second;
		int currentPriority = elements[index].priority;
		elements[index].priority = newPriority;

		if (newPriority < currentPriority) {
			heapifyUp(index);
		} else if (newPriority > currentPriority) {
			heapifyDown(index);
		}
		return true;
	}

	// For debugging
	const std::vector<Node> &getElements() const {
		return elements;
	}
};
