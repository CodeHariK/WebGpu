#include <fstream>
#include <iostream>
#include <string>

#include "PriorityQueue.hpp"
#include "celebi_parse.hpp"
#include "json.hpp"

using json = nlohmann::json;

int main() {
	std::ifstream file("library.celebi"); // open file
	if (!file.is_open()) {
		std::cerr << "Error: could not open file\n";
		return 1;
	}

	try {
		json j;
		file >> j; // directly parse from stream

		Celebi::Data d1;
		Celebi::from_json(j, d1);

		std::cout << d1.items.size() << " items loaded\n";

		Celebi::Data d2 = j.get<Celebi::Data>();
		std::cout << "Parsed " << d1.items.size() << " items\n";

		for (size_t i = 0; i < d1.items.size(); i++) {
			const auto &item = d1.items[i];
			std::cout << "Item " << i << ": " << item.obj << "\n";
		}

		// std::cout << j.dump(2) << '\n'; // pretty print with indentation

	} catch (json::exception &e) {
		std::cerr << "Parse error: " << e.what() << '\n';
	}

	std::string planetJsonStr = R"(
    {
        "planets": ["Mercury", "Venus", "Earth", "Mars",
                    "Jupiter", "Uranus", "Neptune"]
    }
    )";

	try {
		json j = json::parse(planetJsonStr);
		// std::cout << j.dump(2) << '\n';
	} catch (json::exception &e) {
		std::cout << e.what() << std::endl;
	}

	PriorityQueue<std::string> pq;

	pq.insert("apple", 5);
	pq.insert("banana", 2);
	pq.insert("cherry", 8);

	std::cout << "Peek: " << *pq.peek() << "\n"; // banana

	pq.updatePriority("apple", 1);

	for (const auto &node : pq.getElements()) {
		std::cout << "item=" << node.item
				  << " priority=" << node.priority << "\n";
	}

	std::cout << "ExtractMin: " << *pq.extractMin() << "\n"; // apple
	std::cout << "ExtractMin: " << *pq.extractMin() << "\n"; // banana
	std::cout << "ExtractMin: " << *pq.extractMin() << "\n"; // cherry
}
