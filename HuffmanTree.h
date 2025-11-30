#pragma once
#include "BitReader.h"
#include <memory>

struct Node {
	int symbol = -1;
	std::unique_ptr<Node> left = nullptr;
	std::unique_ptr<Node> right = nullptr;
};

class HuffmanTree {
public:
	HuffmanTree() : root(std::make_unique<Node>()) {}

	void insert(int codeword, int n, int symbol);
	int decode(BitReader& bitReader) const;

private:
	std::unique_ptr<Node> root;
};

