#pragma once
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

template <typename T>
class PriorityQueue {
private:
	struct Node {
		T item;
		int priority;
	};

	std::vector<Node> elements;
	std::unordered_map<T, size_t> positions; // item -> index in heap

	void swap(size_t i, size_t j) {
		std::swap(elements[i], elements[j]);
		positions[elements[i].item] = i;
		positions[elements[j].item] = j;
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
		size_t n = elements.size();
		while (true) {
			size_t left = 2 * index + 1;
			size_t right = 2 * index + 2;
			size_t smallest = index;

			if (left < n && elements[left].priority < elements[smallest].priority)
				smallest = left;
			if (right < n && elements[right].priority < elements[smallest].priority)
				smallest = right;

			if (smallest == index)
				break;
			swap(index, smallest);
			index = smallest;
		}
	}

public:
	PriorityQueue() = default;

	// Insert item with priority
	void insert(const T &item, int priority) {
		elements.push_back({ item, priority });
		positions[item] = elements.size() - 1;
		heapifyUp(elements.size() - 1);
	}

	// Extract min item (returns optional)
	std::optional<T> extractMin() {
		if (elements.empty())
			return std::nullopt;

		Node minNode = elements[0];
		positions.erase(minNode.item);

		Node last = elements.back();
		elements.pop_back();

		if (!elements.empty()) {
			elements[0] = last;
			positions[last.item] = 0;
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
	void updatePriority(const T &item, int newPriority) {
		auto it = positions.find(item);
		if (it == positions.end())
			throw std::runtime_error("Item not found in priority queue");

		size_t index = it->second;
		int currentPriority = elements[index].priority;
		elements[index].priority = newPriority;

		if (newPriority < currentPriority) {
			heapifyUp(index);
		} else if (newPriority > currentPriority) {
			heapifyDown(index);
		}
	}

	// For debugging
	const std::vector<Node> &getElements() const {
		return elements;
	}
};
