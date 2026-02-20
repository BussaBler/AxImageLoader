#include "HuffmanTree.h"

void HuffmanTree::insert(int codeword, int n, int symbol) {
	Node* node = root.get();
	for (int i = n - 1; i >= 0; i--) {
		int b = codeword & (1 << i);
		if (b) {
			if (!node->right) {
				node->right = std::make_unique<Node>();
			}
			node = node->right.get();
		}
		else {
			if (!node->left) {
				node->left = std::make_unique<Node>();
			}
			node = node->left.get();
		}
	}
	node->symbol = symbol;
}

int HuffmanTree::decode(BitReader& bitReader) const {
	Node* node = root.get();
	while (node->left || node->right) {
		int b = bitReader.readBit();
		node = b ? node->right.get() : node->left.get();
		if (!node) {
			throw std::runtime_error("Invalid Huffman code");
		}
	}
	return node->symbol;
}
