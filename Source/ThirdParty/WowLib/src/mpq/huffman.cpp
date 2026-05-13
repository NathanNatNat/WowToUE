/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "huffman.h"
#include "bitstream.h"

#include <array>
#include <memory>
#include <stdexcept>
#include <format>

namespace mpq {

// ref: https://en.wikipedia.org/wiki/Huffman_coding
struct LinkedNode {
	int decompressedValue;
	int weight;
	LinkedNode* parent;
	LinkedNode* child0; // Left child (bit 0)
	LinkedNode* next;
	LinkedNode* prev;

	LinkedNode(int decompVal, int weight)
		: decompressedValue(decompVal), weight(weight),
		  parent(nullptr), child0(nullptr), next(nullptr), prev(nullptr) {}

	LinkedNode* child1() const {
		return child0 ? child0->prev : nullptr;
	}

	LinkedNode* insert(LinkedNode* other) {
		if (other->weight <= weight) {
			if (next != nullptr) {
				next->prev = other;
				other->next = next;
			}
			next = other;
			other->prev = this;
			return other;
		}

		if (prev == nullptr) {
			other->prev = nullptr;
			prev = other;
			other->next = this;
		} else {
			prev->insert(other);
		}
		return this;
	}
};

struct NodePool {
	std::vector<std::unique_ptr<LinkedNode>> nodes;

	LinkedNode* alloc(int decompVal, int weight) {
		nodes.push_back(std::make_unique<LinkedNode>(decompVal, weight));
		return nodes.back().get();
	}
};

static const std::array<std::vector<int>, 9> s_prime = {{
	// type 0 - Reserved/not implemented
	[]() {
		std::vector<int> v(256, 0);
		v[0] = 10;
		v[254] = 2;
		return v;
	}(),

	// type 1 - General purpose
	{
		84, 22, 22, 13, 12, 8, 6, 5, 6, 5, 6, 3, 4, 4, 3, 5,
		14, 11, 20, 19, 19, 9, 11, 6, 5, 4, 3, 2, 3, 2, 2, 2,
		13, 7, 9, 6, 6, 4, 3, 2, 4, 3, 3, 3, 3, 3, 2, 2,
		9, 6, 4, 4, 4, 4, 3, 2, 3, 2, 2, 2, 2, 3, 2, 4,
		8, 3, 4, 7, 9, 5, 3, 3, 3, 3, 2, 2, 2, 3, 2, 2,
		3, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 1, 2, 2,
		6, 10, 8, 8, 6, 7, 4, 3, 4, 4, 2, 2, 4, 2, 3, 3,
		4, 3, 7, 7, 9, 6, 4, 3, 3, 2, 1, 2, 2, 2, 2, 2,
		10, 2, 2, 3, 2, 2, 1, 1, 2, 2, 2, 6, 3, 5, 2, 3,
		2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1, 1,
		2, 1, 1, 1, 1, 1, 1, 2, 4, 4, 4, 7, 9, 8, 12, 2,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 3,
		4, 1, 2, 4, 5, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1,
		4, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		2, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1,
		2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2, 2, 6, 75,
	},

	// type 2 - ASCII text
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 39, 0, 0, 35, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		255, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 6, 14, 16, 4,
		6, 8, 5, 4, 4, 3, 3, 2, 2, 3, 3, 1, 1, 2, 1, 1,
		1, 4, 2, 4, 2, 2, 2, 1, 1, 4, 1, 1, 2, 3, 3, 2,
		3, 1, 3, 6, 4, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1,
		1, 41, 7, 22, 18, 64, 10, 10, 17, 37, 1, 3, 23, 16, 38, 42,
		16, 1, 35, 35, 47, 16, 6, 7, 2, 9, 1, 1, 1, 1, 1,
	},

	// type 3 - Binary data
	{
		255, 11, 7, 5, 11, 2, 2, 2, 6, 2, 2, 1, 4, 2, 1, 3,
		9, 1, 1, 1, 3, 4, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1,
		5, 1, 1, 1, 13, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		2, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1,
		10, 4, 2, 1, 6, 3, 2, 1, 1, 1, 1, 1, 3, 1, 1, 1,
		5, 2, 3, 4, 3, 3, 3, 2, 1, 1, 1, 2, 1, 2, 3, 3,
		1, 3, 1, 1, 2, 5, 1, 1, 4, 3, 5, 1, 3, 1, 3, 3,
		2, 1, 4, 3, 10, 6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		2, 2, 1, 10, 2, 5, 1, 1, 2, 7, 2, 23, 1, 5, 1, 1,
		14, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		6, 2, 1, 4, 5, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		7, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1,
		2, 1, 1, 1, 1, 1, 1, 17,
	},

	// type 4 - 16 entries
	{
		255, 251, 152, 154, 132, 133, 99, 100, 62, 62, 34, 34, 19, 19, 24, 23,
	},

	// type 5 - 64 entries
	{
		255, 241, 157, 158, 154, 155, 154, 151, 147, 147, 140, 142, 134, 136, 128, 130,
		124, 124, 114, 115, 105, 107, 95, 96, 85, 86, 74, 75, 64, 65, 55, 55,
		47, 47, 39, 39, 33, 33, 27, 28, 23, 23, 19, 19, 16, 16, 13, 13,
		11, 11, 9, 9, 8, 8, 7, 7, 6, 5, 5, 4, 4, 4, 25, 24,
	},

	// type 6 - 130 entries
	{
		195, 203, 245, 65, 255, 123, 247, 33, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		191, 204, 242, 64, 253, 124, 247, 34, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		122, 70,
	},

	// type 7 - 130 entries
	{
		195, 217, 239, 61, 249, 124, 233, 30, 253, 171, 241, 44, 252, 91, 254, 23,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		189, 217, 236, 61, 245, 125, 232, 29, 251, 174, 240, 44, 251, 92, 255, 24,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		112, 108,
	},

	// type 8 - 130 entries
	{
		186, 197, 218, 51, 227, 109, 216, 24, 229, 148, 218, 35, 223, 74, 209, 16,
		238, 175, 228, 44, 234, 90, 222, 21, 244, 135, 233, 33, 246, 67, 252, 18,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		176, 199, 216, 51, 227, 107, 214, 24, 231, 149, 216, 35, 219, 73, 208, 17,
		233, 178, 226, 43, 232, 92, 221, 21, 241, 135, 231, 32, 247, 68, 255, 19,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		95, 158,
	},
}};

static LinkedNode* build_list(const std::vector<int>& primeData, NodePool& pool) {
	LinkedNode* linked_node = pool.alloc(256, 1)->insert(pool.alloc(257, 1));

	for (size_t decomp_val = 0; decomp_val < primeData.size(); decomp_val++) {
		if (primeData[decomp_val] != 0)
			linked_node = linked_node->insert(pool.alloc(static_cast<int>(decomp_val), primeData[decomp_val]));
	}

	return linked_node;
}

static LinkedNode* build_tree(LinkedNode* tail, NodePool& pool) {
	LinkedNode* current = tail;

	while (current != nullptr && current->prev != nullptr) {
		LinkedNode* node1 = current;
		LinkedNode* node2 = current->prev;

		if (node2 != nullptr) {
			// create parent node with combined weight
			LinkedNode* parent = pool.alloc(0, node1->weight + node2->weight);
			parent->child0 = node1; // left child (bit 0)
			node1->parent = parent;
			node2->parent = parent;

			current->insert(parent);
		} else {
			break;
		}

		current = (current->prev != nullptr && current->prev->prev != nullptr)
			? current->prev->prev : nullptr;
	}

	return current;
}

static LinkedNode* decode(BitStream& input, LinkedNode* head) {
	LinkedNode* current = head;

	// traverse tree until we reach a leaf node
	while (current->child0 != nullptr) {
		const int bit = input.readBits(1);

		if (bit == -1)
			throw std::runtime_error("Unexpected eof");

		// 0 = left child, 1 = right child
		current = (bit == 0) ? current->child0 : current->child1();
	}

	return current;
}

static LinkedNode* insert_node(LinkedNode* tail, int decomp, NodePool& pool);
static void adjust_tree(LinkedNode* newNode);

static LinkedNode* insert_node(LinkedNode* tail, int decomp, NodePool& pool) {
	LinkedNode* node1 = tail;
	LinkedNode* prev = tail->prev;

	LinkedNode* node2 = pool.alloc(node1->decompressedValue, node1->weight);
	node2->parent = node1;

	LinkedNode* newNode = pool.alloc(decomp, 0);
	newNode->parent = node1;

	node1->child0 = newNode;

	tail->next = node2;
	node2->prev = tail;
	newNode->prev = node2;
	node2->next = newNode;

	adjust_tree(newNode);
	adjust_tree(newNode);

	return prev;
}

static void adjust_tree(LinkedNode* newNode) {
	LinkedNode* current = newNode;

	while (current != nullptr) {
		current->weight++;

		LinkedNode* node2 = current;
		LinkedNode* prev = nullptr;

		while (true) {
			prev = node2->prev;
			if (prev != nullptr && prev->weight < current->weight) {
				node2 = prev;
			} else {
				break;
			}
		}

		if (node2 == current) {
			current = current->parent; // no swap, move up
		} else {
			if (node2->prev != nullptr)
				node2->prev->next = node2->next;

			if (node2->next != nullptr)
				node2->next->prev = node2->prev;

			node2->next = current->next;
			node2->prev = current;

			if (current->next != nullptr)
				current->next->prev = node2;

			current->next = node2;

			if (current->prev != nullptr)
				current->prev->next = current->next;

			if (current->next != nullptr)
				current->next->prev = current->prev;

			LinkedNode* next = prev->next;
			current->next = next;
			current->prev = prev;

			if (next != nullptr)
				next->prev = current;

			prev->next = current;

			LinkedNode* parent1 = current->parent;
			LinkedNode* parent2 = node2->parent;

			if (parent1 != nullptr && parent1->child0 == current)
				parent1->child0 = node2;

			if (parent1 != parent2 && parent2 != nullptr && parent2->child0 == node2)
				parent2->child0 = current;

			current->parent = parent2;
			node2->parent = parent1;

			current = current->parent;
		}
	}
}

std::vector<uint8_t> huffman_decomp(std::span<const uint8_t> compressedData) {
	if (compressedData.empty())
		throw std::runtime_error("empty compressed data");

	const int comp_type = static_cast<int>(compressedData[0]);
	if (comp_type == 0)
		throw std::runtime_error("compression type 0 is not currently supported");

	if (comp_type < 0 || comp_type >= static_cast<int>(s_prime.size()))
		throw std::runtime_error(std::format("invalid compression type: {}", comp_type));

	NodePool pool;
	LinkedNode* tail = build_list(s_prime[comp_type], pool);
	LinkedNode* head = build_tree(tail, pool);

	BitStream input(compressedData.subspan(1));

	std::vector<uint8_t> result;
	LinkedNode* currentTail = tail;

	while (true) {
		LinkedNode* node = decode(input, head);
		const int decomp_value = node->decompressedValue;

		if (decomp_value == 256)
			break; // eos

		if (decomp_value == 257) {
			const int literalByte = input.readBits(8);
			if (literalByte == -1)
				throw std::runtime_error("unexpected end of file while reading literal byte");

			result.push_back(static_cast<uint8_t>(literalByte));
			currentTail = insert_node(currentTail, literalByte, pool);
		} else {
			result.push_back(static_cast<uint8_t>(decomp_value));
		}
	}

	return result;
}

} // namespace mpq
